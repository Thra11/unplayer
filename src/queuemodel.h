/*
 * Unplayer
 * Copyright (C) 2015 Alexey Rochev <equeim@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UNPLAYER_QUEUEMODEL_H
#define UNPLAYER_QUEUEMODEL_H

#include <QAbstractListModel>
#include <QQmlParserStatus>

namespace unplayer
{

class Queue;

class QueueModel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(Queue* queue READ queue WRITE setQueue)
public:
    void classBegin();
    void componentComplete();

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    Queue* queue() const;
    void setQueue(Queue *queue);

    Q_INVOKABLE QVariantMap get(int trackIndex) const;
protected:
    QHash<int, QByteArray> roleNames() const;
private:
    void removeTracks(const QList<int> &trackIndexes);
private:
    Queue *m_queue;
};

}

#endif // UNPLAYER_QUEUEMODEL_H
