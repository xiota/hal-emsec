#include "gui/comment_system/widgets/comment_widget.h"
#include "gui/gui_globals.h"
#include "gui/comment_system/comment_entry.h"
#include "gui/comment_system/widgets/comment_item.h"
#include <QDebug>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include <QAction>
#include <QIcon>
#include <QSpacerItem>
#include <QToolBar>
#include "gui/toolbar/toolbar.h"
#include "gui/searchbar/searchbar.h"

namespace hal
{
    CommentWidget::CommentWidget(QWidget *parent) : QWidget(parent)
    {
        //mTopLayout = new QVBoxLayout(this);
        mTopLayout = new QGridLayout(this);
        mTopLayout->setMargin(0);
        mTopLayout->setSpacing(0);
        mSearchbar = new Searchbar();
        mSearchbar->hide();

        // top bar
        // 1. Option
        mHeaderLayout = new QHBoxLayout();
        mNewCommentButton = new QToolButton();
        mNewCommentButton->setIcon(QIcon(":/icons/plus"));
        mNewCommentButton->setIconSize(QSize(25,25));
        mSearchButton = new QToolButton();
        mSearchButton->setIcon(QIcon(":/icons/search"));
        mSearchButton->setIconSize(QSize(25,25));
        mHeaderLayout->addWidget(mNewCommentButton);// alignleft without spacer
        mHeaderLayout->addSpacerItem(new QSpacerItem(0,0, QSizePolicy::Expanding, QSizePolicy::Preferred));
        mHeaderLayout->addWidget(mSearchButton);// alignright without spacer
        mHeaderLayout->addWidget(mSearchbar);


//        mToolbar = new QToolBar("Title?");
//        //mToolbar = new Toolbar();
//        QAction* newCommentAction = new QAction(QIcon(":/icons/plus"), "New Comment");
//        QAction* mSearchAction = new QAction(QIcon(":/icons/search"), "Search");
//        mToolbar->addAction(newCommentAction);
//        //mToolbar->addSpacer();
//        QWidget* spacer = new QWidget(); // must be used when using QToolBar
//        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
//        mToolbar->addWidget(spacer);
//        mToolbar->addAction(mSearchAction);
//        mToolbar->addWidget(mSearchbar);
//        mSearchbar->show();
//        mToolbar->setMinimumHeight(mSearchbar->height());// can be set if QToolbar is used (not custom)
//        mTopLayout->addWidget(mToolbar);

        // comment part
        mCommentsLayout = new QVBoxLayout();
        mCommentsLayout->setSpacing(0);
        mCommentsLayout->setMargin(0);
        mScrollArea = new QScrollArea();
        mScrollArea->setWidgetResizable(true);//important as it seems so that the child widget will expandd if possible
        mCommentsContainer = new QWidget();
        mCommentsContainer->show();
        mCommentsContainer->setLayout(mCommentsLayout);
        mCommentsContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        mScrollArea->setWidget(mCommentsContainer);
        mScrollArea->show();
        //mCommentsContainer->setMinimumSize(500,200);
        //mTopLayout->addWidget(mScrollArea);

        // test fillings
//        QLabel* commentImposter = new QLabel("Perhaps i will be a comment someday?");
//        commentImposter->setWordWrap(true);
//        //commentImposter->show();
//        //mScrollArea->setWidget(commentImposter);
//        mCommentsLayout->addWidget(commentImposter);
//        mCommentsLayout->addWidget(new QLabel("II will be another Comment (hopefully)"));
        mTopLayout->addLayout(mHeaderLayout,0,0);
        mTopLayout->addWidget(mScrollArea, 1, 0);
        //mTopLayout->addItem(new QSpacerItem(0,0, QSizePolicy::Expanding, QSizePolicy::Expanding),2,0);
        mTopLayout->setRowStretch(1, 0);


        // testing, remove later
        setMinimumWidth(350);
        resize(350, 300);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
        // connections / logic
        connect(mSearchButton, &QAbstractButton::clicked, this, &CommentWidget::handleSearchbarTriggered);
        connect(mSearchbar, &Searchbar::searchIconClicked, this, &CommentWidget::handleSearchbarTriggered);
    }

    CommentWidget::~CommentWidget()
    {
        qDebug() << "CommentWidget::~CommentWidget()";
    }

    void CommentWidget::setItem(CommentItem *item)
    {
        mCommentsLayout->addWidget(item, 0, Qt::AlignTop);
    }

    void CommentWidget::addHackySpacer()
    {
                QWidget* hackySpacerItem = new QWidget();
                hackySpacerItem->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                mCommentsLayout->addWidget(hackySpacerItem);
    }

    void CommentWidget::nodeChanged(const Node& nd)
    {
        mScrollArea->setWidget(createAndFillCommentContainerFactory(nd));
    }

    QWidget *CommentWidget::createAndFillCommentContainerFactory(const Node &nd)
    {
        QWidget* container = new QWidget();
        QVBoxLayout* containerLayout = new QVBoxLayout(container);
        containerLayout->setSpacing(0);
        containerLayout->setMargin(0);

        auto commentList = gCommentManager->getEntriesForNode(nd);

        // create new items
        for(const auto& entry : commentList)
        {
            CommentItem* item = new CommentItem(entry, container);
            //item->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
            containerLayout->addWidget(item);
            item->show();
        }

        containerLayout->addStretch();
        return container;
    }

    void CommentWidget::handleSearchbarTriggered()
    {
        if(mSearchbar->isHidden())
        {
            mSearchButton->hide();
            mSearchbar->show();
        }
        else
        {
            mSearchbar->hide();;
            mSearchButton->show();
        }
        //mSearchbar->isHidden() ? mSearchbar->show() : mSearchbar->hide();
    }

    void CommentWidget::handleNewCommentTriggered()
    {
        qDebug() << "A new comment wants to be created!";
    }

}
