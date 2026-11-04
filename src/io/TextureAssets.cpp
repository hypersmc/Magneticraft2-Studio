#include "TextureAssets.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QHash>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <limits>
#include <zlib.h>

namespace studio {
namespace {
constexpr qsizetype kMaxArchiveSize=512LL*1024*1024;
constexpr quint32 kMaxPngBytes=8*1024*1024;
constexpr qsizetype kMaxImportedBytes=36LL*1024*1024;
constexpr int kMaxTextures=256;

QString normalizeId(const std::string& id) {
    const QString raw=QString::fromStdString(id).trimmed();
    if(raw.isEmpty() || raw.startsWith("textures/"))return {};
    if(raw.contains("..") || raw.contains('\\') || raw.contains('%'))return {};
    const QString path=raw.contains(':')?raw:"minecraft:"+raw;
    static const QRegularExpression accepted("^[a-z0-9_.-]+:[a-z0-9_./-]+$");
    return accepted.match(path).hasMatch() ? path : QString();
}
QString archivePath(const QString& id) {
    const int delimiter=id.indexOf(':');
    return "assets/"+id.left(delimiter)+"/textures/"+id.mid(delimiter+1)+".png";
}
bool validPng(const QByteArray& bytes,QString& reason) {
    if(bytes.isEmpty() || bytes.size()>kMaxPngBytes) {
        reason="PNG is empty or exceeds the 8 MiB resource limit";
        return false;
    }
    const auto image=QImage::fromData(bytes,"PNG");
    if(image.isNull() || image.width()>4096 || image.height()>4096) {
        reason="Texture is not a valid PNG (maximum 4096×4096 pixels)";
        return false;
    }
    return true;
}
quint16 read16(const QByteArray& bytes,qsizetype at) {
    if(at<0 || at+2>bytes.size())return 0;
    const auto* p=reinterpret_cast<const unsigned char*>(bytes.constData()+at);
    return quint16(p[0]) | (quint16(p[1])<<8);
}
quint32 read32(const QByteArray& bytes,qsizetype at) {
    if(at<0 || at+4>bytes.size())return 0;
    const auto* p=reinterpret_cast<const unsigned char*>(bytes.constData()+at);
    return quint32(p[0])|(quint32(p[1])<<8)|(quint32(p[2])<<16)|(quint32(p[3])<<24);
}
struct ZipEntry {quint32 offset=0, compressed=0, uncompressed=0, crc=0;quint16 method=0,flags=0;};

bool listZipEntries(const QByteArray& archive,const QSet<QString>& paths,
                    QHash<QString,ZipEntry>& found, QString& error) {
    if(archive.size()<22){error="Not a ZIP/JAR archive";return false;}
    const qsizetype start=std::max(qsizetype(0),archive.size()-qsizetype(65557));
    qsizetype eocd=-1;
    for(qsizetype at=archive.size()-22;at>=start;at--) {
        if(read32(archive,at)==0x06054b50 && at+22+read16(archive,at+20)==archive.size()){
            eocd=at;break;
        }
    }
    if(eocd<0){error="ZIP/JAR end-of-directory record was not found";return false;}
    if(read16(archive,eocd+4)!=0 || read16(archive,eocd+6)!=0){
        error="Multi-disk ZIP archives are not supported";return false;
    }
    const quint16 entries=read16(archive,eocd+10);
    const quint32 directorySize=read32(archive,eocd+12),directoryOffset=read32(archive,eocd+16);
    if(entries==65535 || directorySize==0xffffffffu || directoryOffset==0xffffffffu){
        error="ZIP64 archives are not supported";return false;
    }
    if(qsizetype(directoryOffset)+directorySize>eocd){
        error="ZIP/JAR central directory is out of bounds";return false;
    }
    qsizetype at=directoryOffset;
    for(unsigned n=0;n<entries;n++) {
        if(at+46>archive.size() || read32(archive,at)!=0x02014b50){
            error="ZIP/JAR directory entry is damaged";return false;
        }
        const quint16 nameLen=read16(archive,at+28),extraLen=read16(archive,at+30),commentLen=read16(archive,at+32);
        const qsizetype next=at+46+qsizetype(nameLen)+extraLen+commentLen;
        if(next>archive.size() || next>qsizetype(directoryOffset)+directorySize){
            error="ZIP/JAR entry extends beyond its directory";return false;
        }
        const QString name=QString::fromUtf8(archive.constData()+at+46,nameLen);
        if(paths.isEmpty() || paths.contains(name)) {
            const ZipEntry e{read32(archive,at+42),read32(archive,at+20),
                             read32(archive,at+24),read32(archive,at+16),
                             read16(archive,at+10),read16(archive,at+8)};
            found.insert(name,e);
        }
        at=next;
    }
    return true;
}

QByteArray extractZipEntry(const QByteArray& archive,const ZipEntry& e,QString& error,
                           qsizetype maxBytes=8*1024*1024) {
    const qsizetype local=e.offset;
    if(local+30>archive.size()||read32(archive,local)!=0x04034b50){
        error="ZIP/JAR entry has an invalid local header";return {};
    }
    if((e.flags&1)!=0){error="Encrypted ZIP entries are not supported";return {};}
    if(e.uncompressed==0 || e.uncompressed>quint64(maxBytes) ||
       e.compressed>quint64(std::max<qsizetype>(maxBytes,1))) {
        error="Archive asset exceeds the safe per-resource size limit";return {};
    }
    if(e.method!=0 && e.method!=8){error="ZIP/JAR entry uses an unsupported compression method";return {};}
    const qsizetype start=local+30+qsizetype(read16(archive,local+26))+read16(archive,local+28);
    if(start<0 || start+e.compressed>archive.size()){
        error="ZIP/JAR compressed texture exceeds archive bounds";return {};
    }
    QByteArray output;
    if(e.method==0){
        if(e.compressed!=e.uncompressed){error="Malformed stored ZIP entry";return {};}
        output=archive.mid(start,e.uncompressed);
    }else{
        if(e.compressed>std::numeric_limits<uInt>::max() || e.uncompressed>std::numeric_limits<uInt>::max()){
            error="ZIP/JAR entry is too large to decompress";return {};
        }
        output.resize(e.uncompressed);
        z_stream stream{};
        stream.next_in=reinterpret_cast<Bytef*>(const_cast<char*>(archive.constData()+start));
        stream.avail_in=e.compressed;
        stream.next_out=reinterpret_cast<Bytef*>(output.data());
        stream.avail_out=e.uncompressed;
        if(inflateInit2(&stream,-MAX_WBITS)!=Z_OK){error="Could not initialize ZIP deflate decoder";return {};}
        const int status=inflate(&stream,Z_FINISH);
        inflateEnd(&stream);
        if(status!=Z_STREAM_END || stream.total_out!=e.uncompressed || stream.total_in!=e.compressed){
            error="ZIP/JAR texture decompression failed";return {};
        }
    }
    auto check=crc32(0L,Z_NULL,0);
    check=crc32(check,reinterpret_cast<const Bytef*>(output.constData()),uInt(output.size()));
    if(quint32(check)!=e.crc){error="ZIP/JAR PNG failed CRC validation";return {};}
    return output;
}
} // namespace
struct AssetSource::Impl {
    QByteArray zip;
    QHash<QString,ZipEntry> entries;
    QDir directory;
    bool zipped=false,valid=false;
};
AssetSource::AssetSource():impl_(std::make_unique<Impl>()){}
AssetSource::~AssetSource()=default;

bool AssetSource::open(const QString& sourcePath,QString& error){
    error.clear();impl_=std::make_unique<Impl>();
    const QFileInfo info(sourcePath);
    if(info.isFile()){
        if(!QStringList{".jar",".zip"}.contains("."+info.suffix().toLower())){
            error="Select a local mod/client JAR, resource-pack ZIP, or assets directory";return false;
        }
        if(info.size()>kMaxArchiveSize){
            error="Local archive is larger than the 512 MiB safety limit";return false;
        }
        QFile file(sourcePath);
        if(!file.open(QIODevice::ReadOnly)){error=file.errorString();return false;}
        impl_->zip=file.readAll();
        if(!listZipEntries(impl_->zip,{},impl_->entries,error)){
            impl_->zip.clear();return false;
        }
        impl_->zipped=true;impl_->valid=true;return true;
    }
    if(!info.isDir()){
        error="Selected asset source is not an existing directory or .jar/.zip file";
        return false;
    }
    impl_->directory=QDir(sourcePath);
    if(!impl_->directory.exists("assets") &&
        impl_->directory.dirName()!="assets"){
        error="Directory must contain assets/ or be the assets/ directory";
        return false;
    }
    impl_->valid=true;
    return true;
}
bool AssetSource::read(const QString& relative,QByteArray& out,QString& error,
                       qsizetype maxBytes) const{
    out.clear();error.clear();
    if(!impl_->valid){error="Asset source is not open";return false;}
    if(!relative.startsWith("assets/")||relative.contains("..")||
       relative.contains('\\')||relative.contains('%')||
       relative.size()>512||maxBytes<1||maxBytes>8*1024*1024){
        error="Rejected unsafe or unsupported Minecraft asset path";return false;
    }
    if(impl_->zipped){
        const auto it=impl_->entries.constFind(relative);
        if(it==impl_->entries.constEnd())return false;
        out=extractZipEntry(impl_->zip,it.value(),error,maxBytes);
        return !out.isEmpty();
    }
    QString name=impl_->directory.filePath(relative);
    if(!QFileInfo(name).isFile() && impl_->directory.dirName()=="assets")
        name=impl_->directory.filePath(relative.mid(7));
    const QFileInfo info(name);
    if(!info.isFile())return false;
    if(info.size()<1||info.size()>maxBytes){
        error="Asset file exceeds its safe size limit";return false;
    }
    QFile file(name);
    if(!file.open(QIODevice::ReadOnly)){error=file.errorString();return false;}
    out=file.readAll();
    return !out.isEmpty();
}

QStringList referencedTextureIds(const Project& project) {
    QSet<QString> ids;
    for(const auto& part:project.parts) {
        if(!part.texture.empty())ids.insert(normalizeId(part.texture));
        for(const auto& face:part.faces)if(!face.texture.empty())ids.insert(normalizeId(face.texture));
    }
    ids.remove(QString());
    QStringList out=ids.values();out.sort(Qt::CaseSensitive);
    return out;
}
QStringList missingTextureIds(const Project& project) {
    QStringList missing;
    for(const QString& id:referencedTextureIds(project)) {
        if(project.textures.find(id.toStdString())==project.textures.end())missing.append(id);
    }
    return missing;
}

TextureAssetReport importTextureAssets(Project& project,const QString& sourcePath,bool overwrite){
    TextureAssetReport report;
    const QStringList required=referencedTextureIds(project);
    QStringList wanted;
    qsizetype allocated=0;
    for(const auto& [id,bytes]:project.textures){
        Q_UNUSED(id);
        allocated+=(qsizetype(bytes.size())*3/4);
    }
    for(const QString& id:required){
        if(!overwrite && project.textures.count(id.toStdString()))report.alreadyPresent.append(id);
        else wanted.append(id);
    }
    if(wanted.isEmpty()){report.missing=missingTextureIds(project);return report;}
    AssetSource source;
    if(!source.open(sourcePath,report.error))return report;
    for(const QString& id:wanted){
        const QString relative=archivePath(id);
        QByteArray png;
        QString warning;
        source.read(relative,png,warning,kMaxPngBytes);
        if(!png.isEmpty() && !validPng(png,warning))png.clear();
        if(!png.isEmpty()) {
            const qsizetype existing=project.textures.count(id.toStdString())?
                qsizetype(project.textures.at(id.toStdString()).size()*3/4):0;
            const qsizetype future=allocated-existing+png.size();
            if(future>kMaxImportedBytes){warning="Project would exceed the 36 MiB embedded texture budget";png.clear();}
            else if(!project.textures.count(id.toStdString()) && project.textures.size()>=kMaxTextures){
                warning="Project would exceed 256 embedded textures";png.clear();
            }else{
                project.textures[id.toStdString()]=png.toBase64().toStdString();
                allocated=future;
                report.imported.append(id);
            }
        }
        if(!warning.isEmpty())report.warnings.append(id+": "+warning);
    }
    report.missing=missingTextureIds(project);
    return report;
}
} // namespace studio
