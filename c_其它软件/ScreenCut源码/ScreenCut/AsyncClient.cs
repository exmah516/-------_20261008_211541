using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

namespace ScreenCut {
    public class DataBuffer {
        public const int size = 4096;
        public byte[] buffer = new byte[size];
    }

    public class AsyncClient {
        public bool IsConnected { get; private set; } = false;
        public EndPoint Endpoint { get; private set; }
        public Socket Socket { get; private set; }
        public ProtocolType Protocol { get; private set; }

        public delegate void OnConnectedHandler(EndPoint ep);
        public delegate void OnConnectFailedHandler(EndPoint ep, string error);
        public delegate void OnDisconnectedHandler(EndPoint ep);
        public delegate void OnReadHandler(byte[] data);
        public delegate void OnReadFailedHandler();
        public delegate void OnWriteHandler(int length);
        public delegate void OnWriteFailedHandler(object data);

        public event OnConnectedHandler OnConnected;
        public event OnConnectFailedHandler OnConnectFailed;
        public event OnDisconnectedHandler OnDisconnected;
        public event OnReadHandler OnRead;
        public event OnReadFailedHandler OnReadFailed;
        public event OnWriteHandler OnWrite;
        public event OnWriteFailedHandler OnWriteFailed;

        public AsyncClient(ProtocolType protocol, int port, IPAddress addr) {
            Protocol = protocol;
            Endpoint = new IPEndPoint(addr, port);
        }

        private void MakeSocket() {
            if (Protocol == ProtocolType.Tcp) {
                Socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, Protocol);
            } else if (Protocol == ProtocolType.Udp) {
                Socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, Protocol);
            } else {
                throw new Exception($"Protocol {Protocol} is unsuppoted now.");
            }
            Socket.SendTimeout = 500;
            Socket.ReceiveTimeout = 500;
            Socket.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
        }


        public void Connect() {
            if (Socket == null) {
                MakeSocket();
            }
            if (!Socket.Connected) {
                Socket.BeginConnect(Endpoint, new AsyncCallback(ConnectCallback), null);
            }
        }

        public void Bind() {
            if (Socket == null) {
                MakeSocket();
            }
            if (Protocol == ProtocolType.Udp) {
                Socket.Bind(Endpoint);
            }
        }

        public void Close() {
            if (Protocol == ProtocolType.Tcp) {
                if (Socket != null) {
                    new Thread(() => {
                        // 关闭Socket之前，首选需要把双方的Socket Shutdown掉
                        Socket.Shutdown(SocketShutdown.Both);
                        // Shutdown掉Socket后主线程停止10ms，保证Socket的Shutdown完成
                        Thread.Sleep(10);
                        // 关闭客户端Socket,清理资源
                        Socket.Close();

                        Socket = null;
                        IsConnected = false;
                        OnDisconnected?.Invoke(Endpoint);
                    }).Start();
                }
            }
        }

        public void Receive() {
            DataBuffer buffer = new DataBuffer();
            if (Protocol == ProtocolType.Tcp) {
                if (!Socket.Connected)
                    return;
                Socket.BeginReceive(buffer.buffer, 0, DataBuffer.size, 0, new AsyncCallback(ReceiveCallback), buffer);

            } else if (Protocol == ProtocolType.Udp) {
                EndPoint remote = Endpoint;
                Socket.BeginReceiveFrom(buffer.buffer, 0, DataBuffer.size, 0, ref remote, new AsyncCallback(ReceiveCallback), buffer);
            }
        }

        public void Send(string data) {
            if (!Socket.Connected)
                return;
            byte[] byteData = Encoding.UTF8.GetBytes(data);
            Send(byteData);
        }

        public void Send(byte[] data) {
            Socket.BeginSend(data, 0, data.Length, 0, new AsyncCallback(SendCallback), data);
        }

        private void ConnectCallback(IAsyncResult ar) {
            string error = "";
            try {
                Socket.EndConnect(ar);
            } catch (Exception e) {
                error = e.Message;
                if (error.Length == 0) {
                    return;
                }
            }

            if (Socket.Connected) {
                IsConnected = true;
                OnConnected?.Invoke(Endpoint);
                return;
            }

            OnConnectFailed?.Invoke(Endpoint, error);
        }

        private void ReceiveCallback(IAsyncResult ar) {
            try {
                int bytesRead = 0;
                DataBuffer buffer = (DataBuffer)ar.AsyncState;
                bytesRead = Socket.EndReceive(ar);
                if (bytesRead > 0) {
                    byte[] buf = new byte[bytesRead];
                    Array.Copy(buffer.buffer, 0, buf, 0, buf.Length);
                    OnRead?.Invoke(buf);
                    Receive();
                    return;
                }
            } catch {
            }

            OnReadFailed?.Invoke();
        }

        private void SendCallback(IAsyncResult ar) {
            byte[] buffer = (byte[])ar.AsyncState;
            try {
                int bytesSent = Socket.EndSend(ar);
                if (bytesSent == buffer.Length) {
                    OnWrite?.Invoke(bytesSent);
                    return;
                }
            } catch {
            }

            OnWriteFailed?.Invoke(buffer);
        }
    }
}
