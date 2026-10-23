using System.IO;
using System.Text;
using MasterConsole.Services;

namespace VI.Control;

public sealed record StartupSelection(bool IsSimulation, RemoteLinkSettings Settings, string Notice)
{
    public static StartupSelection Parse(string[] args, string baseDirectory)
    {
        string host = new RemoteLinkSettings().Host;
        string? tokenPath = null;
        bool simulation = false;
        for (int i = 0; i < args.Length; i++)
        {
            if (args[i] == "--sim") simulation = true;
            else if (args[i] == "--host" && i + 1 < args.Length) host = args[++i];
            else if (args[i] == "--token" && i + 1 < args.Length) tokenPath = args[++i];
        }
        if (simulation)
            return new(true, new() { Host = host }, "模拟模式 · 命令不会发给从端");

        var candidates = new List<string>();
        if (!string.IsNullOrEmpty(tokenPath)) candidates.Add(tokenPath);
        string? directory = baseDirectory;
        for (int level = 0; level < 10 && !string.IsNullOrEmpty(directory); level++)
        {
            candidates.Add(Path.Combine(directory, "remote.token"));
            candidates.Add(Path.Combine(directory, "config", "remote.token"));
            directory = Path.GetDirectoryName(directory.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar));
        }
        string? rejected = null;
        foreach (string candidate in candidates)
        {
            if (!File.Exists(candidate)) continue;
            byte[] token = Encoding.UTF8.GetBytes(File.ReadAllText(candidate, Encoding.UTF8).Trim());
            if (token.Length < 16) { rejected = candidate; continue; }
            return new(false, new() { Host = host, Token = token }, "");
        }
        return new(true, new() { Host = host }, rejected is null
            ? "模拟模式 · 未找到 remote.token，命令不会发给从端"
            : "模拟模式 · 密钥不足 16 字节，命令不会发给从端：" + rejected);
    }
}
