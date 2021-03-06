/*
    Copyright (C) 2012-2016 Lawo GmbH (http://www.lawo.com).
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#include "NodeView.h"
#include "gadget/Node.h"
#include "util/StringConverter.h"

NodeView::NodeView(QWidget* parent, gadget::Node* node)
    : QFrame(parent)
    , m_view(new Ui::NodeView())
    , m_node(node)
{
    m_view->setupUi(this);
    updateUi();
}

NodeView::~NodeView()
{
    delete m_view;
}

void NodeView::updateUi()
{
    auto const identifier = util::StringConverter::toUtf8QString(m_node->identifier());
    auto const description = util::StringConverter::toUtf8QString(m_node->description());
    auto const schema = util::StringConverter::toUtf8QString(m_node->schema());
    auto const number = QVariant(m_node->number()).toString();
    auto const isOnline = m_node->isOnline();
    auto const isUnmounted = !m_node->isMounted();

    if (m_view->identifierText->text() != identifier)
        m_view->identifierText->setText(identifier);

    if (m_view->descriptionText->text() != description)
        m_view->descriptionText->setText(description);

    if (m_view->schemaText->text() != schema)
        m_view->schemaText->setText(schema);

    if (m_view->numberText->text() != number)
        m_view->numberText->setText(number);

    if (m_view->isOnlineCheckBox->isChecked() != isOnline)
        m_view->isOnlineCheckBox->setChecked(isOnline);

    if (m_view->unmountCheckBox->isChecked() != isUnmounted)
        m_view->unmountCheckBox->setChecked(isUnmounted);
}

void NodeView::updateDescription()
{
    auto const description = util::StringConverter::toUtf8StdString(m_view->descriptionText->text());
    m_node->setDescription(description);
}

void NodeView::updateSchema()
{
    auto const schema = util::StringConverter::toUtf8StdString(m_view->schemaText->text());
    m_node->setSchema(schema);
}

void NodeView::updateOnlineState(bool state)
{
    m_node->setIsOnline(state);
}

void NodeView::updateUnmountState(bool state)
{
    if (state)
        m_node->unmount();
    else
        m_node->mount();

    m_view->isOnlineCheckBox->setChecked(m_node->isOnline());
}
