using System;
using System.Globalization;
using System.IO;
using System.Text;

namespace ScreenCut
{
    internal static class StreamingLog
    {
        private const long MaxFileBytes = 2 * 1024 * 1024;
        private const int ArchiveCount = 3;
        private static readonly object Gate = new object();
        private static readonly Encoding Utf8 = new UTF8Encoding(false);
        internal static string LogPath { get; } = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "streaming.log");

        internal static string Write(string text)
        {
            text = StreamingHost.Redact(text);
            string entry = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss.fff ", CultureInfo.InvariantCulture) + text + Environment.NewLine;
            lock (Gate)
            {
                try
                {
                    var file = new FileInfo(LogPath);
                    if (file.Exists && file.Length > 0 && file.Length + Utf8.GetByteCount(entry) > MaxFileBytes)
                    {
                        string Archive(int n) => Path.Combine(Path.GetDirectoryName(LogPath), "streaming." + n + ".log");
                        File.Delete(Archive(ArchiveCount));
                        for (int i = ArchiveCount - 1; i >= 1; i--)
                            if (File.Exists(Archive(i))) File.Move(Archive(i), Archive(i + 1));
                        File.Move(LogPath, Archive(1));
                    }
                    // 逐条追加后关闭文件，不在内存积攒历史或等待退出才落盘。
                    File.AppendAllText(LogPath, entry, Utf8);
                }
                catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException)
                {
                    return text + "（日志写入失败：" + LogPath + "；" + ex.Message + "）";
                }
            }
            return text;
        }
    }
}
