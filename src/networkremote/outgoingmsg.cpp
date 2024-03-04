#include "outgoingmsg.h"
#include "core/player.h"
#include "core/logging.h"
#include "playlist/playlistmanager.h"

OutgoingMsg::OutgoingMsg(Application *app, QObject *parent)
    : QObject{parent},
      app_(app),
      msg_(new nw::remote::Message),
      song_(new nw::remote::SongMetadata),
      responeSong_(new nw::remote::ResponseSongMetadata),
      player_(app_->player())
{
}

OutgoingMsg::~OutgoingMsg()
{
}

void OutgoingMsg::ProcessMsg(QTcpSocket * socket, qint32 msgType)
{
  socket_ = socket;
  msgType_ = msgType;
  msg_->Clear();

  switch (msgType_) {
    case nw::remote::MSG_TYPE_CONNECT:
      SendCurrentTrackInfo();
      break;
  default:
      qLog(Debug) << "Unknow Message Type " << msgType_;
      break;
  }
  SendMsg();
}

void OutgoingMsg::SendCurrentTrackInfo()
{
  msg_->Clear();
  song_->Clear();
  responeSong_->Clear();

  playerState_ = player_->engine()->state();
  playlist_ = app_->playlist_manager()->current();


  if (playerState_ == EngineBase::State::Playing){
    Song currentSong = playlist_->current_item_metadata();
    song_->mutable_title()->assign(currentSong.PrettyTitle().toStdString());
    song_->mutable_album()->assign(currentSong.album().toStdString());
    song_->mutable_artist()->assign(currentSong.artist().toStdString());
    song_->mutable_albumartist()->assign(currentSong.albumartist().toStdString());
    song_->set_track(currentSong.track());
    song_->mutable_stryear()->assign(currentSong.PrettyYear().toStdString());
    song_->mutable_genre()->assign(currentSong.genre().toStdString());
    song_->set_playcount(currentSong.playcount());
    song_->mutable_songlength()->assign(currentSong.PrettyLength().toStdString());

    msg_->set_type(nw::remote::MSG_TYPE_PLAY);
    msg_->mutable_response_song_metadata()->set_player_state(nw::remote::PLAYER_STATUS_PLAYING);
    qLog(Debug) << "Current Title with Artist " << currentSong.PrettyTitleWithArtist();
  }
  else {
    /* NOTE:  TODO
     * I couldn't figure out how to get the song data if the song wasn't playing
     *
     * */
    msg_->set_type(nw::remote::MSG_TYPE_UNSPECIFIED);
    msg_->mutable_response_song_metadata()->set_player_state(nw::remote::PLAYER_STATUS_UNSPECIFIED);
  }
}

void OutgoingMsg::SendMsg()
{

}
