#include "StartPage.hpp"

#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace studio {
namespace {

QLabel* label(const QString& text, const char* role, QWidget* parent) {
    auto* item = new QLabel(text,parent);
    item->setObjectName(role);
    item->setWordWrap(QString(role)=="StartCardDescription" ||
                      QString(role)=="StartExampleDescription" ||
                      QString(role)=="StartHeroSubtitle");
    item->setTextFormat(Qt::PlainText);
    return item;
}

QFrame* projectCard(QWidget* parent, const QString& icon, const QString& name,
                    const QString& description, const QString& badge,
                    const QString& action, const char* buttonId, QPushButton*& button) {
    auto* card = new QFrame(parent);
    card->setObjectName("StartCard");
    card->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Minimum);
    auto* box = new QVBoxLayout(card);
    box->setContentsMargins(24,23,24,23);
    box->setSpacing(10);
    auto* top = new QHBoxLayout;
    auto* glyph = label(icon,"StartCardGlyph",card);
    glyph->setFixedWidth(44);
    top->addWidget(glyph);
    top->addWidget(label(badge,"StartBadge",card),1,Qt::AlignVCenter);
    box->addLayout(top);
    box->addWidget(label(name,"StartCardTitle",card));
    box->addWidget(label(description,"StartCardDescription",card));
    box->addStretch();
    button = new QPushButton(action,card);
    button->setObjectName(buttonId);
    button->setAccessibleName(action);
    button->setCursor(Qt::PointingHandCursor);
    if(QString(buttonId)=="NewMultiblockCard")button->setProperty("primaryStartAction",true);
    box->addWidget(button);
    card->setMinimumHeight(223);
    return card;
}

QFrame* exampleCard(QWidget* parent, const QString& eyebrow, const QString& name,
                    const QString& description, const QString& action,
                    const char* buttonId, QPushButton*& button) {
    auto* card=new QFrame(parent);
    card->setObjectName("StartExampleCard");
    auto* box=new QVBoxLayout(card);
    box->setContentsMargins(18,16,18,16);
    box->setSpacing(8);
    box->addWidget(label(eyebrow,"StartExampleTag",card));
    box->addWidget(label(name,"StartExampleTitle",card));
    box->addWidget(label(description,"StartExampleDescription",card));
    box->addStretch();
    button=new QPushButton(action,card);
    button->setObjectName(buttonId);
    button->setCursor(Qt::PointingHandCursor);
    box->addWidget(button);
    card->setMinimumHeight(181);
    return card;
}
} // namespace

StartPage::StartPage(QWidget* parent):QWidget(parent) {
    setObjectName("StartPage");
    auto* outer=new QVBoxLayout(this);
    outer->setContentsMargins(0,0,0,0);
    auto* scroll=new QScrollArea(this);
    scroll->setObjectName("StartScrollArea");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* surface=new QWidget(scroll);
    surface->setObjectName("StartSurface");
    auto* content=new QVBoxLayout(surface);
    content->setContentsMargins(46,37,46,44);
    content->setSpacing(15);

    auto* welcome=label("MAGNETICRAFT²  /  STUDIO","StartEyebrow",surface);
    content->addWidget(welcome);
    content->addWidget(label("Make something worth building.","StartHeroTitle",surface));
    content->addWidget(label("A Minecraft modeling workspace for individual blocks, large multiblocks and everything in between.",
                             "StartHeroSubtitle",surface));
    content->addSpacing(15);
    content->addWidget(label("NEW PROJECT","StartSectionTitle",surface));
    auto* createGrid=new QGridLayout;
    createGrid->setHorizontalSpacing(16);
    createGrid->setVerticalSpacing(16);
    createGrid->setColumnStretch(0,1);
    createGrid->setColumnStretch(1,1);
    QPushButton *javaBtn=nullptr,*multiBtn=nullptr;
    createGrid->addWidget(projectCard(surface,"▣","Java block model",
        "A normal Minecraft Java block model. Edit a single block with geometry in the familiar 3 × 3 × 3 modeling envelope (element coordinates −16 to 32).",
        "MINECRAFT JAVA  ·  3 × 3 × 3","Create Java block","NewJavaBlockCard",javaBtn),0,0);
    createGrid->addWidget(projectCard(surface,"⬡","Magneticraft2 multiblock",
        "Design the construction layout and the formed model together. Pick the width, height and depth—without the Java block model size restriction.",
        "LARGE MACHINES  ·  CUSTOM DIMENSIONS","Create multiblock","NewMultiblockCard",multiBtn),0,1);
    content->addLayout(createGrid);
    connect(javaBtn,&QPushButton::clicked,this,&StartPage::createJavaBlockRequested);
    connect(multiBtn,&QPushButton::clicked,this,&StartPage::createMultiblockRequested);

    content->addSpacing(14);
    auto* titleRow=new QHBoxLayout;
    titleRow->addWidget(label("EXAMPLES  /  LEARN BY EDITING","StartSectionTitle",surface));
    titleRow->addStretch(1);
    auto* openBtn=new QPushButton("Open an existing project…",surface);
    openBtn->setObjectName("OpenProjectFromHome");
    titleRow->addWidget(openBtn);
    content->addLayout(titleRow);
    connect(openBtn,&QPushButton::clicked,this,&StartPage::openProjectRequested);

    auto* examples=new QGridLayout;
    examples->setHorizontalSpacing(16);examples->setVerticalSpacing(14);
    examples->setColumnStretch(0,1);examples->setColumnStretch(1,1);
    QPushButton *washer=nullptr,*sifter=nullptr,*workshop=nullptr,*casing=nullptr;
    examples->addWidget(exampleCard(surface,"MAGNETICRAFT2  ·  IMPORTED MODEL",
        "Mechanical Ore Washer",
        "Open the real construction layout and editable static geometry from the mod. Rotating parts and PNG assets need further integration.",
        "Explore Ore Washer","ExampleOreWasher",washer),0,0);
    examples->addWidget(exampleCard(surface,"MAGNETICRAFT2  ·  CONSTRUCTION",
        "Mechanical Sifter",
        "Explore the real Sifter block layout. Its formed appearance is not included in this example yet.",
        "Explore Sifter","ExampleSifter",sifter),0,1);
    examples->addWidget(exampleCard(surface,"TUTORIAL  ·  SIMPLE MULTIBLOCK",
        "Workshop demonstration",
        "A small illustrated machine for learning the basic geometry, selection and UV controls. Not the real Ore Washer.",
        "Explore workshop","ExampleWorkshop",workshop),1,0);
    examples->addWidget(exampleCard(surface,"MINECRAFT JAVA  ·  STARTER BLOCK",
        "Copper casing",
        "A one-block cuboid starter using the Java block model limits. Experiment with material and face UVs.",
        "Explore Java block","ExampleCopperBlock",casing),1,1);
    content->addLayout(examples);
    connect(washer,&QPushButton::clicked,this,[this](){emit exampleRequested("washer");});
    connect(sifter,&QPushButton::clicked,this,[this](){emit exampleRequested("sifter");});
    connect(workshop,&QPushButton::clicked,this,[this](){emit exampleRequested("workshop");});
    connect(casing,&QPushButton::clicked,this,[this](){emit exampleRequested("casing");});

    content->addSpacing(17);
    content->addWidget(label("RECENT PROJECTS","StartSectionTitle",surface));
    recentList_=new QListWidget(surface);
    recentList_->setObjectName("RecentProjectsList");
    recentList_->setAlternatingRowColors(false);
    recentList_->setMinimumHeight(104);
    recentList_->setMaximumHeight(215);
    recentList_->setToolTip("Open one of your recently saved Studio projects");
    content->addWidget(recentList_);
    connect(recentList_,&QListWidget::itemActivated,this,[this](QListWidgetItem* item){
        if(item && (item->flags() & Qt::ItemIsEnabled))emit recentProjectRequested(item->data(Qt::UserRole).toString());
    });
    content->addStretch();
    scroll->setWidget(surface);
    outer->addWidget(scroll);
}

void StartPage::setRecentProjects(const QStringList& paths) {
    recentList_->clear();
    if(paths.isEmpty()){
        auto* empty=new QListWidgetItem("No recent projects yet. Save a project to see it here.",recentList_);
        empty->setFlags(Qt::NoItemFlags);
        return;
    }
    for(const QString& path:paths){
        QFileInfo file(path);
        if(!file.exists()||!file.isFile())continue;
        auto* item=new QListWidgetItem(file.fileName()+"   ·   "+QDir::toNativeSeparators(file.absolutePath()),recentList_);
        item->setData(Qt::UserRole,file.absoluteFilePath());
        item->setToolTip(file.absoluteFilePath());
    }
    if(recentList_->count()==0){
        auto* empty=new QListWidgetItem("No saved projects found on disk.",recentList_);
        empty->setFlags(Qt::NoItemFlags);
    }
}

} // namespace studio
