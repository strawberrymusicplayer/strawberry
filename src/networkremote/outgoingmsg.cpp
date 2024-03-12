#include "outgoingmsg.h"
#include "core/player.h"
#include "core/logging.h"
#include "playlist/playlistmanager.h"

OutgoingMsg::OutgoingMsg(Application *app, QObject *parent)
    : QObject{parent},
      app_(app),
      msg_(new nw::remote::Message),
      responeSong_(new nw::remote::ResponseSongMetadata)
{
}

OutgoingMsg::~OutgoingMsg()
{
}

void OutgoingMsg::Init(QTcpSocket *socket, SharedPtr<Player> player)
{
  socket_ = socket;
  player_ = player;
}


void OutgoingMsg::SendCurrentTrackInfo()
{
  msg_->Clear();
  song_ = new nw::remote::SongMetadata;
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

    msg_->set_type(nw::remote::MSG_TYPE_REPLY_SONG_INFO);
    msg_->mutable_response_song_metadata()->set_player_state(nw::remote::PLAYER_STATUS_PLAYING);
    msg_->mutable_response_song_metadata()->set_allocated_song_metadata(song_);

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
  SendMsg();
}

void OutgoingMsg::SendMsg()
{
  std::string  msgOut;

  msg_->SerializeToString(&msgOut);


  bytesOut_ = msg_->ByteSizeLong();

  if(socket_->isWritable())
  {

    socket_->write(QByteArray::fromStdString(msgOut));
    qInfo() << socket_->bytesToWrite() << " bytes written to socket " << socket_->socketDescriptor();
    statusOk_ = true;
    msg_->Clear();
  }
  else
  {
    statusOk_ = false;
  }
}

