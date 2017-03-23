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

#include "player.h"

#include <QTimer>

#include <AudioResourceQt>
#include <MprisPlayer>

#include "queue.h"

namespace unplayer
{

Player::Player()
    : m_audioResource(new AudioResourceQt::AudioResource(this, AudioResourceQt::AudioResource::MediaType)),
      m_pipeline(gst_element_factory_make("playbin", NULL)),
      m_bus(gst_element_get_bus(m_pipeline)),
      m_mprisPlayer(new MprisPlayer(this)),
      m_queue(new Queue(this)),
      m_playing(false),
      m_positionTimer(new QTimer(this))
{
    connect(m_audioResource, &AudioResourceQt::AudioResource::acquiredChanged, this, &Player::onAudioResourceAcquiredChanged);

    gst_bus_add_watch(m_bus, &Player::onGstBusMessage, this);

    connect(m_queue, &Queue::currentTrackChanged, this, &Player::onQueueCurrentTrackChanged);

    connect(m_positionTimer, &QTimer::timeout, this, &Player::positionChanged);
    m_positionTimer->setInterval(500);

    m_mprisPlayer->setServiceName("unplayer");
    m_mprisPlayer->setCanControl(true);
    connect(m_mprisPlayer, &MprisPlayer::playRequested, this, &Player::play);
    connect(m_mprisPlayer, &MprisPlayer::pauseRequested, this, &Player::pause);
    connect(m_mprisPlayer, &MprisPlayer::nextRequested, m_queue, &Queue::next);
    connect(m_mprisPlayer, &MprisPlayer::previousRequested, m_queue, &Queue::previous);
}

Player::~Player()
{
    gst_object_unref(m_bus);
    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(m_pipeline));
}

Queue* Player::queue() const
{
    return m_queue;
}

bool Player::isPlaying() const
{
    return m_playing;
}

qint64 Player::position() const
{
    GstFormat format = GST_FORMAT_TIME;
    gint64 cur;
    gboolean result = gst_element_query_position(m_pipeline, format, &cur);
    if (result)
        // msecs
        return cur / 1000000;

    return 0;
}

// msecs
void Player::setPosition(qint64 newPosition)
{
    gst_element_seek_simple(m_pipeline,
                            GST_FORMAT_TIME,
                            static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                            newPosition * 1000000);
}

void Player::play()
{
    m_audioResource->acquire();

    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    setPlaying(true);
    m_positionTimer->start();

    m_mprisPlayer->setPlaybackStatus(Mpris::Playing);
}

void Player::pause()
{
    gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    setPlaying(false);
    m_positionTimer->stop();

    m_audioResource->release();

    m_mprisPlayer->setPlaybackStatus(Mpris::Paused);
}

void Player::stop()
{
    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    setPlaying(false);
    m_positionTimer->stop();
    emit positionChanged();

    m_audioResource->release();

    m_mprisPlayer->setPlaybackStatus(Mpris::Stopped);
}

void Player::setPlaying(bool playing)
{
    m_playing = playing;
    emit playingChanged();
}

void Player::onAudioResourceAcquiredChanged()
{
    if (!m_audioResource->isAcquired()) {
        if (m_playing)
            pause();
    }
}

gboolean Player::onGstBusMessage(GstBus*, GstMessage *message, gpointer user_data)
{
    Player *instance = static_cast<Player*>(user_data);

    if (message->type == GST_MESSAGE_EOS) {
        if (!instance->queue()->nextOnEos())
            instance->stop();
    } else if (message->type == GST_MESSAGE_ERROR) {
        instance->stop();
    }

    return TRUE;
}

void Player::onQueueCurrentTrackChanged()
{
    if (m_queue->currentIndex() == -1) {
        stop();
        g_object_set(G_OBJECT(m_pipeline), "uri", "", NULL);

        m_mprisPlayer->setCanPlay(false);
        m_mprisPlayer->setCanPause(false);

        m_mprisPlayer->setCanGoNext(false);
        m_mprisPlayer->setCanGoPrevious(false);

        m_mprisPlayer->setPlaybackStatus(Mpris::Stopped);

        m_mprisPlayer->setMetadata(QVariantMap());

        return;
    }

    m_mprisPlayer->setCanPlay(true);
    m_mprisPlayer->setCanPause(true);

    m_mprisPlayer->setCanGoNext(true);
    m_mprisPlayer->setCanGoPrevious(true);

    const QueueTrack *track = m_queue->tracks().at(m_queue->currentIndex());

    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    emit positionChanged();
    g_object_set(G_OBJECT(m_pipeline), "uri", track->url.toUtf8().data(), NULL);
    play();

    QVariantMap metadata;
    metadata.insert(Mpris::metadataToString(Mpris::Title), track->title);
    metadata.insert(Mpris::metadataToString(Mpris::Artist), track->artist);
    metadata.insert(Mpris::metadataToString(Mpris::Album), track->album);
    m_mprisPlayer->setMetadata(metadata);
}

}
