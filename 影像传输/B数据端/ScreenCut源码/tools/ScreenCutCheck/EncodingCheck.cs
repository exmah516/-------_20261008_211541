using System;
using System.Collections.Specialized;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using ScreenCut;

internal static class EncodingCheck
{
    public static int Run()
    {
        string work = Path.Combine(Path.GetTempPath(), "VI.Imaging.Encoding-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(work);
        try
        {
            var settings = StreamingSettings.Load(new NameValueCollection());
            const int frames = 200;
            string arguments = settings.FfmpegArguments(Rectangle.Empty, true).Replace("-re ", "");
            int output = arguments.LastIndexOf("-f rtsp ", StringComparison.Ordinal);
            if (output < 0) throw new Exception("未找到实际编码管线的输出。");
            RunFfmpeg(work, arguments.Substring(0, output) + "-frames:v " + frames + " -y -f matroska sample.mkv");
            RunFfmpeg(work, "-hide_banner -loglevel error -f lavfi -i testsrc2=size=960x540:rate=25 " +
                "-i sample.mkv -filter_complex \"[0:v][1:v]ssim=stats_file=ssim.txt\" -frames:v " + frames + " -f null -");
            var scores = File.ReadLines(Path.Combine(work, "ssim.txt"))
                .Select(line => Regex.Match(line, @"All:([\d.]+)"))
                .Where(match => match.Success)
                .Take(frames)
                .Select(match => double.Parse(match.Groups[1].Value, CultureInfo.InvariantCulture)).ToArray();
            if (scores.Length != frames) throw new Exception("画质检查缺帧。");
            // 同一动态素材逐帧对照，直接检测每秒关键帧造成的画质跳变。
            double keyframeStep = Enumerable.Range(1, (frames - 1) / settings.VideoFps)
                .Max(second => Math.Abs(scores[second * settings.VideoFps] - scores[second * settings.VideoFps - 1]));
            Console.WriteLine("ENCODING_QUALITY frames={0} meanSsim={1:F6} maxKeyframeStep={2:F6}", frames, scores.Average(), keyframeStep);
            if (keyframeStep > 0.004 || scores.Average() < 0.958)
                throw new Exception("每秒关键帧仍有明显画质跳变，或平均画质低于回归基线。");
            Console.WriteLine("PASS 每秒关键帧画质稳定，无需放宽播放器重连条件。");
            return 0;
        }
        catch (Exception ex) { Console.WriteLine("FAIL " + StreamingHost.Redact(ex.Message)); return 1; }
        finally { Directory.Delete(work, true); }
    }

    private static void RunFfmpeg(string work, string arguments)
    {
        using (var process = new Process { StartInfo = new ProcessStartInfo(StreamingSettings.FindRuntime("ffmpeg.exe"), arguments) {
            UseShellExecute = false, CreateNoWindow = true, WorkingDirectory = work,
            RedirectStandardError = true, RedirectStandardOutput = true
        } })
        {
            process.Start();
            var error = process.StandardError.ReadToEndAsync();
            var output = process.StandardOutput.ReadToEndAsync();
            if (!process.WaitForExit(20000)) { process.Kill(); process.WaitForExit(); throw new TimeoutException("FFmpeg 画质检查超时。"); }
            if (process.ExitCode != 0) throw new Exception(error.GetAwaiter().GetResult());
            output.GetAwaiter().GetResult();
        }
    }
}
