using System;
using System.Collections.Specialized;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using ScreenCut;

public sealed class LoggingCheck : MarshalByRefObject
{
    public static int Run()
    {
        string root = Path.Combine(Path.GetTempPath(), "VI.Imaging.LogCheck-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        var domain = AppDomain.CreateDomain("log-check", null, new AppDomainSetup { ApplicationBase = root });
        try
        {
            var check = (LoggingCheck)domain.CreateInstanceFromAndUnwrap(typeof(LoggingCheck).Assembly.Location, typeof(LoggingCheck).FullName);
            check.Verify();
            Console.WriteLine("ALL LOGGING CHECKS PASSED");
            return 0;
        }
        catch (Exception ex) { Console.WriteLine("FAIL " + ex); return 1; }
        finally { AppDomain.Unload(domain); Directory.Delete(root, true); }
    }

    private static void Check(bool condition, string message)
    {
        if (!condition) throw new Exception(message);
        Console.WriteLine("PASS " + message);
    }

    public void Verify()
    {
        // 独立 AppDomain 的目录模拟 EXE 目录，不覆盖开发机或部署现场的日志。
        using (var host = new StreamingHost(StreamingSettings.Load(new NameValueCollection())))
        {
            var log = (Action<string>)Delegate.CreateDelegate(typeof(Action<string>), host,
                typeof(StreamingHost).GetMethod("Log", BindingFlags.NonPublic | BindingFlags.Instance));
            string path = host.LogPath;
            Check(Path.GetDirectoryName(path) == AppDomain.CurrentDomain.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar), "log uses executable directory");
            log("正常提示：媒体就绪");
            string text = File.ReadAllText(path);
            Check(text.Contains("正常提示：媒体就绪"), "normal notice is persisted immediately without UI subscribers");
            Check(Regex.IsMatch(text, @"^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3} "), "timestamp includes date and milliseconds");
            string status = null;
            host.Status += line => status = line;
            log("[mediamtx.exe] ERR rtsp://camera:test-secret@127.0.0.1/stream");
            Check(!File.ReadAllText(path).Contains("test-secret") && !status.Contains("test-secret"), "credentials redacted in log and status");
            Parallel.For(0, 200, i => log("parallel-notice-" + i));
            string[] notices = File.ReadLines(path).Where(l => l.Contains("parallel-notice-")).ToArray();
            Check(notices.Length == 200 && notices.Distinct().Count() == 200, "concurrent notices retained without line corruption");

            const long limit = 2 * 1024 * 1024;
            for (int i = 0; i < 5; i++)
            {
                File.WriteAllText(path, "previous-" + i + Environment.NewLine);
                using (var file = new FileStream(path, FileMode.Open, FileAccess.Write)) file.SetLength(limit);
                log("after-rotation-" + i);
            }
            Check(File.ReadAllText(path).Contains("after-rotation-4"), "current log survives rollover");
            for (int i = 1; i <= 3; i++)
                using (var file = new StreamReader(Path.Combine(Path.GetDirectoryName(path), "streaming." + i + ".log")))
                    Check(file.ReadLine() == "previous-" + (5 - i), "archive " + i + " keeps expected generation");
            var files = new DirectoryInfo(Path.GetDirectoryName(path)).GetFiles("streaming*.log");
            Check(files.Length == 4 && files.Sum(f => f.Length) <= 4 * limit, "disk retention bounded to current plus three 2 MiB files");

            File.SetAttributes(path, FileAttributes.ReadOnly);
            try
            {
                log("notice during write failure");
                Check(status.Contains("日志写入失败"), "unwritable log reports failure without stopping media callback");
            }
            finally { File.SetAttributes(path, FileAttributes.Normal); }
        }
    }
}
