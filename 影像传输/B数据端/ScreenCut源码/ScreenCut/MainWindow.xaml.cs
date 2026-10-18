using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace ScreenCut {
    /// <summary>
    /// MainWindow.xaml 的交互逻辑
    /// </summary>
    public partial class MainWindow : Window {
        private AsyncClient udp_imgSend = null;

        private Socket udpSerCmdRecv = null;
        // 接收命令的端口
        private Int32 mSerCmdRecvPort = 0;
        // 发送截取图像的数量
        private Int16 mImgSendCount = 1;

        private bool[] mvIsFastSend = { false, false };

        //// 发送间隔计数
        private Int16[] mvSendImageCount = { 0, 0 };

        // 连续读取超时
        private Int16 miTimeoutKeepCount = 0;

        public MainWindow() {
            InitializeComponent();
        }

        // 截图测试
        private void Button_Click_Cut1(object sender, RoutedEventArgs e) {
            Bitmap bitmap = null;
            try {
                int iLeft = Convert.ToInt32(txtLeft.Text);
                int iTop = Convert.ToInt32(txtTop.Text);
                int iWidth = Convert.ToInt32(txtWidth.Text);
                int iHeight = Convert.ToInt32(txtHeight.Text);
                ImageCut(ref bitmap, iLeft, iTop, iWidth, iHeight);
            } catch {

            }

            pictureBox.Source = GetImageSouce(bitmap);
        }

        private void Button_Click_Cut2(object sender, RoutedEventArgs e) {
            Bitmap bitmap = null;
            try {
                int iLeft = Convert.ToInt32(txtLeft1.Text);
                int iTop = Convert.ToInt32(txtTop1.Text);
                int iWidth = Convert.ToInt32(txtWidth1.Text);
                int iHeight = Convert.ToInt32(txtHeight1.Text);
                ImageCut(ref bitmap, iLeft, iTop, iWidth, iHeight);
            } catch {

            }

            pictureBox.Source = GetImageSouce(bitmap);
        }

        // 启动持续传图
        private void Button_Click_1(object sender, RoutedEventArgs e) {
            udp_imgSend = new AsyncClient(ProtocolType.Udp, 36661, IPAddress.Parse(txtIP.Text));
            udp_imgSend.Connect();

            // 命令接收用于接收是否快速发送影像
            udpSerCmdRecv = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
            udpSerCmdRecv.ReceiveTimeout = 200;
            //绑定IP和端口
            udpSerCmdRecv.Bind(new IPEndPoint(IPAddress.Any, 36662));
            mSerCmdRecvPort = 36662;
            mImgSendCount = 2;
            // 启动命令读取定时器
            var timerCmd = new System.Windows.Threading.DispatcherTimer();
            timerCmd.Tick += TimerCmd_Tick;
            timerCmd.Interval = new TimeSpan(0, 0, 0, 0, 300);
            timerCmd.Start();



            var timerImage = new System.Windows.Threading.DispatcherTimer();
            timerImage.Tick += TimerImage_Tick;
            timerImage.Interval = new TimeSpan(0, 0, 0, 0, 300);
            timerImage.Start();

            btn_keep1.IsEnabled = false;
            btn_keep2.IsEnabled = false;
        }

        // 持续传图2
        private void Button_Click_2(object sender, RoutedEventArgs e) {
            udp_imgSend = new AsyncClient(ProtocolType.Udp, 36663, IPAddress.Parse(txtIP.Text));
            udp_imgSend.Connect();

            // 命令接收用于接收是否快速发送影像
            udpSerCmdRecv = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
            udpSerCmdRecv.ReceiveTimeout = 200;
            //绑定IP和端口
            udpSerCmdRecv.Bind(new IPEndPoint(IPAddress.Any, 36664));
            mSerCmdRecvPort = 36664;
            mImgSendCount = 1;
            // 启动命令读取定时器
            var timerCmd = new System.Windows.Threading.DispatcherTimer();
            timerCmd.Tick += TimerCmd_Tick;
            timerCmd.Interval = new TimeSpan(0, 0, 0, 0, 300);
            timerCmd.Start();

            var timerImage = new System.Windows.Threading.DispatcherTimer();
            timerImage.Tick += TimerImage_Tick;
            timerImage.Interval = new TimeSpan(0, 0, 0, 0, 300);
            timerImage.Start();

            btn_keep1.IsEnabled = false;
            btn_keep2.IsEnabled = false;
        }



        private BitmapSource GetImageSouce(Bitmap bitmap) {
            BitmapSource img;
            IntPtr hBitmap;

            hBitmap = bitmap.GetHbitmap();
            img = Imaging.CreateBitmapSourceFromHBitmap(
                hBitmap,
                IntPtr.Zero,
                Int32Rect.Empty,
                BitmapSizeOptions.FromEmptyOptions());
            return img;
        }


        private void ImageCut (ref Bitmap bitmap, int iLeft, int iTop, int iWidth, int iHeight) {
            if ( null == bitmap) {
                bitmap = new Bitmap(iWidth, iHeight, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
            }

            // 调用PrintWindow函数进行截图
            Graphics graphics = Graphics.FromImage(bitmap);
            graphics.CopyFromScreen(iLeft, iTop, 0, 0, new System.Drawing.Size(iWidth, iHeight));
        }

        private void TimerImage_Tick(object sender, EventArgs e) {
            //if (!mbIsFastSend && 0 != ++miSendImageCount % 20) {
            //    return;
            //}
            
            //miSendImageCount = 0;

            sizeShow.Text = "";
            for ( int j = 0; j < mImgSendCount; j++ ) {
                // 确定快慢速发送
                if (!mvIsFastSend[j] && 0 != ++mvSendImageCount[j] %20 ) continue;
                mvSendImageCount[j] = 0;

                Bitmap bitmap = null;
                try {
                    TextBox[] ltwh = { txtLeft , txtTop , txtWidth , txtHeight, txtLeft1, txtTop1, txtWidth1, txtHeight1 };

                    int iLeft = Convert.ToInt32(ltwh[j*4].Text);
                    int iTop = Convert.ToInt32(ltwh[j*4+1].Text);
                    int iWidth = Convert.ToInt32(ltwh[j*4+2].Text);
                    int iHeight = Convert.ToInt32(ltwh[j*4+3].Text);
                    ImageCut(ref bitmap, iLeft, iTop, iWidth, iHeight);
                } catch {
                    continue;

                }


                // bitmap.Save()
                //System.IO.MemoryStream mStream = new System.IO.MemoryStream();
                //bitmap.Save()
                //bitmap.
                //            ByteArrayOutputStream stream = new ByteArrayOutputStream();

                // 设置保存图像质量
                ImageCodecInfo[] codecs = ImageCodecInfo.GetImageEncoders();
                ImageCodecInfo ici = null;
                foreach (ImageCodecInfo codec in codecs) {
                    if (codec.MimeType == "image/jpeg")
                        ici = codec;
                }
                EncoderParameters ep = new EncoderParameters();
                try {
                    int level = Convert.ToInt32(0 == j ? p.Text : p1.Text); //图像质量 1-100的范围
                    ep.Param[0] = new EncoderParameter(System.Drawing.Imaging.Encoder.Quality, (long)level);
                } catch {
                    continue;
                }


                // 1.先将BitMap转成内存流
                MemoryStream ms = new MemoryStream();
                //bitmap.Save(ms, ImageFormat.Jpeg);
                bitmap.Save(ms, ici, ep);
                ms.Seek(0, SeekOrigin.Begin);
                // 2.再将内存流转成byte[]并返回
                byte[] bytes = new byte[ms.Length];
                ms.Read(bytes, 0, bytes.Length);
                ms.Dispose();

                // 发送数据长度
                byte[] send = new byte[5];
                send[0] = (byte)j;
                Int32 iDataSize = bytes.Length;
                for (int i = 4; i >= 1; i--) {
                    send[i] = (byte)(iDataSize % 256);
                    iDataSize = iDataSize / 256;
                }
                udp_imgSend.Send(send);


                // 分包发送数据
                iDataSize = bytes.Length;
                Int32 iSendDataPos = 0;

                const Int16 iDataBlock = 8192;
                while (iDataSize > iSendDataPos) {
                    byte[] bTemp = null;
                    if (iDataSize - iSendDataPos > iDataBlock) {
                        bTemp = new byte[iDataBlock];
                        Array.Copy(bytes, iSendDataPos, bTemp, 0, iDataBlock);
                    } else {
                        bTemp = new byte[iDataSize - iSendDataPos];
                        Array.Copy(bytes, iSendDataPos, bTemp, 0, iDataSize - iSendDataPos);
                    }

                    iSendDataPos += iDataBlock;
                    udp_imgSend.Send(bTemp);
                }

                sizeShow.Text += bytes.Length.ToString() + "   ";

            }
            Console.WriteLine(DateTime.Now.ToString());
        }

        private void TimerCmd_Tick(object sender, EventArgs e) {
            EndPoint remoteEndPoint = new IPEndPoint(IPAddress.Any, mSerCmdRecvPort);
            //设置一个64k的字节数组作为缓存
            byte[] data = new byte[10];

            try {
                int length = udpSerCmdRecv.ReceiveFrom(data, ref remoteEndPoint);//此方法把数据来源ip、port放到第二个参数中
                if (2 == length) {
                    miTimeoutKeepCount = 0;
                    if ( 1 == data[0] ) {
                        mvIsFastSend[0] = true;
                        mvIsFastSend[1] = false;
                    } else if ( 1 == data[1] ) {
                        mvIsFastSend[0] = false;
                        mvIsFastSend[1] = true;
                    }
                }
                //if ( 1 == data[length-1] ) {
                //    mbIsFastSend = true;
                //} else {
                //    mbIsFastSend = false;
                //}
            } catch {
                if ( miTimeoutKeepCount++ > 5 ) {
                    mvIsFastSend[0] = false;
                    mvIsFastSend[1] = false;
                }
            }


        }
    }
}
