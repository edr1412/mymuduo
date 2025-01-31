// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPCONNECTION_H
#define MUDUO_NET_TCPCONNECTION_H

#include <muduo/base/noncopyable.h>
#include <muduo/base/StringPiece.h>
#include <muduo/base/Types.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>

#include <any>
#include <memory>
#include <atomic>

// struct tcp_info is in <netinet/tcp.h>
struct tcp_info;

namespace muduo
{
namespace net
{

class Channel;
class EventLoop;
class Socket;

///
/// TCP connection, for both client and server usage.
///
/// This is an interface class, so don't expose too much details.
/// TcpConnection是muduo里唯一默认使用shared_ptr来管理的class，也是唯一继承enable_shared_from_this的class，这源于其模糊的生命期
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection>
{
 public:
  /// Constructs a TcpConnection with a connected sockfd
  ///
  /// User should not create this object.
  TcpConnection(EventLoop* loop,
                const string& name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
  ~TcpConnection();

  EventLoop* getLoop() const { return loop_; }
  const string& name() const { return name_; }
  const InetAddress& localAddress() const { return localAddr_; }
  const InetAddress& peerAddress() const { return peerAddr_; }
  bool connected() const { return state_ == StateE::kConnected; }
  bool disconnected() const { return state_ == StateE::kDisconnected; }
  // return true if success.
  bool getTcpInfo(struct tcp_info*) const;
  string getTcpInfoString() const;

  void send(const char* message); // 避免二义性
  void send(const string& message); // 避免二义性
  void send(string&& message); // 右值引用版本
  void send(const void* message, int len);
  void send(const std::string_view& message);
  // void send(Buffer&& message); 
  void send(Buffer* message);  // this one will swap data
  void shutdown(); // NOT thread safe, no simultaneous calling
  // void shutdownAndForceCloseAfter(double seconds); // NOT thread safe, no simultaneous calling
  void forceClose();
  void forceCloseWithDelay(double seconds);
  void setTcpNoDelay(bool on);
  // reading or not
  void startRead();
  void stopRead();
  bool isReading() const { return reading_; }; // NOT thread safe, may race with start/stopReadInLoop

  void setContext(const std::any& context)
  { context_ = context; }

  const std::any& getContext() const
  { return context_; }

  std::any* getMutableContext()
  { return &context_; }

  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; }

  void setMessageCallback(const MessageCallback& cb)
  { messageCallback_ = cb; }

  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  { writeCompleteCallback_ = cb; }

  // 假设存在 client —— proxy —— server 的业务场景，设置
  // HighWaterMarkCallback 可以避免因 server 和 client 之间发送数据
  // 速度的差异而导致 proxy 的 Buffer 被撑爆
  void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
  { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

  /// Advanced interface
  Buffer* inputBuffer()
  { return &inputBuffer_; }

  Buffer* outputBuffer()
  { return &outputBuffer_; }

  /// Internal use only.
  /// closeCallback_ 主要是提供给 TcpServer 和 TcpClient 使用的，
  /// 用来通知他们移除所持有的 TcpConnectionPtr 的，而不是提供给普通用户使用的
  void setCloseCallback(const CloseCallback& cb)
  { closeCallback_ = cb; }

  // called when TcpServer accepts a new connection
  void connectEstablished();   // should be called only once
  // called when TcpServer has removed me from its map
  void connectDestroyed();  // should be called only once

 private:
  enum class StateE {
    kDisconnected,
    kConnecting,
    kConnected,
    kDisconnecting,
  };
  void setState(StateE s) { state_.store(s); }
  const char* stateToString() const;
  // 处理函数
  void handleRead(Timestamp receiveTime);
  void handleWrite();
  void handleClose();
  void handleError();
  void sendInLoop(string&& message);
  void sendInLoop(const std::string_view& message);
  void sendInLoop(const void* message, size_t len);

  void shutdownInLoop();
  // void shutdownAndForceCloseInLoop(double seconds);
  void forceCloseInLoop();
  void startReadInLoop();
  void stopReadInLoop();

  EventLoop* loop_;
  const string name_;
  std::atomic<StateE> state_;
  bool reading_;
  // we don't expose those classes to client.
  std::unique_ptr<Socket> socket_;
  std::unique_ptr<Channel> channel_;
  const InetAddress localAddr_;
  const InetAddress peerAddr_;

  // 用户自定义的回调函数，用户传给TcpServer，TcpServer再传给TcpConnection
  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_; // 又名：低水位回调
  // 用户自定义的回调函数，用户直接传给TcpConnection
  HighWaterMarkCallback highWaterMarkCallback_;

  CloseCallback closeCallback_;
  size_t highWaterMark_;
  Buffer inputBuffer_;
  Buffer outputBuffer_; // FIXME: use list<Buffer> as output buffer.
  // context 用于保存与 connection 绑定的任意数据，
  // 这样客户代码不必继承 TCPConnection 也可以 attach 自己的状态。
  std::any context_;
  // FIXME: creationTime_, lastReceiveTime_
  //        bytesReceived_, bytesSent_
};

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_TCPCONNECTION_H
