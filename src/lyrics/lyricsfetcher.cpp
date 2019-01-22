/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QTimer>
#include <QString>

#include "core/logging.h"
#include "core/song.h"
#include "lyricsfetcher.h"
#include "lyricsfetchersearch.h"

const int LyricsFetcher::kMaxConcurrentRequests = 5;
const QRegExp LyricsFetcher::kRemoveNonAlpha("[^a-zA-Z0-9\\d\\s]");

LyricsFetcher::LyricsFetcher(LyricsProviders *lyrics_providers, QObject *parent)
    : QObject(parent),
      lyrics_providers_(lyrics_providers),
      next_id_(0),
      request_starter_(new QTimer(this))
  {

  request_starter_->setInterval(500);
  connect(request_starter_, SIGNAL(timeout()), SLOT(StartRequests()));

}

quint64 LyricsFetcher::Search(const QString &artist, const QString &album, const QString &title) {

  LyricsSearchRequest request;
  request.artist = artist;
  request.album = album;
  request.album.remove(kRemoveNonAlpha);
  request.album.remove(Song::kTitleRemoveMisc);
  request.title = title;
  request.title.remove(kRemoveNonAlpha);
  request.title.remove(Song::kTitleRemoveMisc);
  request.id = next_id_++;
  AddRequest(request);

  return request.id;

}

void LyricsFetcher::AddRequest(const LyricsSearchRequest &req) {

  queued_requests_.enqueue(req);

  if (!request_starter_->isActive()) request_starter_->start();

  if (active_requests_.size() < kMaxConcurrentRequests) StartRequests();

}

void LyricsFetcher::Clear() {

  queued_requests_.clear();

  for (LyricsFetcherSearch *search : active_requests_.values()) {
    search->Cancel();
    search->deleteLater();
  }
  active_requests_.clear();

}

void LyricsFetcher::StartRequests() {

  if (queued_requests_.isEmpty()) {
    request_starter_->stop();
    return;
  }

  while (!queued_requests_.isEmpty() && active_requests_.size() < kMaxConcurrentRequests) {

    LyricsSearchRequest request = queued_requests_.dequeue();

    LyricsFetcherSearch *search = new LyricsFetcherSearch(request, this);
    active_requests_.insert(request.id, search);

    connect(search, SIGNAL(SearchFinished(quint64, LyricsSearchResults)), SLOT(SingleSearchFinished(quint64, LyricsSearchResults)));
    connect(search, SIGNAL(LyricsFetched(quint64, const QString&)), SLOT(SingleLyricsFetched(quint64, const QString&)));

    search->Start(lyrics_providers_);
  }

}

void LyricsFetcher::SingleSearchFinished(quint64 request_id, LyricsSearchResults results) {

  LyricsFetcherSearch *search = active_requests_.take(request_id);
  if (!search) return;
  search->deleteLater();
  emit SearchFinished(request_id, results);

}

void LyricsFetcher::SingleLyricsFetched(quint64 request_id, const QString &lyrics) {

  LyricsFetcherSearch *search = active_requests_.take(request_id);
  if (!search) return;
  search->deleteLater();
  emit LyricsFetched(request_id, lyrics);

}
