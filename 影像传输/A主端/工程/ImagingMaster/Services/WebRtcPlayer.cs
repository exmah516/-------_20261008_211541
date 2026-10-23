using System;
using System.IO;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using Microsoft.Web.WebView2.Core;
using Microsoft.Web.WebView2.Wpf;

namespace MasterConsole.Services
{
    public sealed class WebRtcPlayer : IDisposable
    {
        public const string Origin = "http://vi-imaging.local";
        private readonly WebView2 _view;
        private bool _disposed;
        private bool _ready;
        private string _selected = "dsa";
        public WebRtcPlayer(WebView2 view) { _view = view; }
        public async Task InitializeAsync(StreamEndpoint[] streams, string userDataFolder = null)
        {
            string playerPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Player");
            if (!File.Exists(Path.Combine(playerPath, "index.html"))) throw new FileNotFoundException("缺少 Player 播放器文件夹。");
            string profile = userDataFolder ?? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "VI.Imaging", "WebView2");
            var environment = await CoreWebView2Environment.CreateAsync(null, profile,
                new CoreWebView2EnvironmentOptions("--autoplay-policy=no-user-gesture-required"));
            if (_disposed) return;
            await _view.EnsureCoreWebView2Async(environment);
            if (_disposed) return;
            var core = _view.CoreWebView2;
            core.Settings.AreDefaultContextMenusEnabled = false;
            core.Settings.IsStatusBarEnabled = false;
            core.Settings.IsZoomControlEnabled = false;
            core.Settings.AreDefaultScriptDialogsEnabled = false;
            core.Settings.AreHostObjectsAllowed = false;
            core.SetVirtualHostNameToFolderMapping("vi-imaging.local", playerPath, CoreWebView2HostResourceAccessKind.DenyCors);
            core.NavigationStarting += (s, e) => { if (!e.Uri.StartsWith(Origin + "/", StringComparison.OrdinalIgnoreCase)) e.Cancel = true; };
            core.NewWindowRequested += (s, e) => e.Handled = true;
            core.DownloadStarting += (s, e) => e.Cancel = true;
            var loaded = new TaskCompletionSource<bool>();
            EventHandler<CoreWebView2NavigationCompletedEventArgs> handler = (s, e) => {
                if (e.IsSuccess) loaded.TrySetResult(true);
                else loaded.TrySetException(new IOException("播放器页面加载失败：" + e.WebErrorStatus));
            };
            core.NavigationCompleted += handler;
            try
            {
                core.Navigate(Origin + "/index.html");
                if (await Task.WhenAny(loaded.Task, Task.Delay(10000)) != loaded.Task) throw new TimeoutException("播放器页面加载超时。");
                await loaded.Task;
                if (!_disposed)
                {
                    await core.ExecuteScriptAsync("window.player.start(" + new JavaScriptSerializer().Serialize(streams) + ")");
                    _ready = true;
                    await SelectAsync(_selected);
                }
            }
            finally { core.NavigationCompleted -= handler; }
        }
        public Task SelectAsync(string key)
        {
            _selected = key;
            if (_disposed || !_ready) return Task.CompletedTask;
            return _view.CoreWebView2.ExecuteScriptAsync("window.player.select(" + new JavaScriptSerializer().Serialize(key) + ")");
        }
        public Task<string> SnapshotAsync() => _view.CoreWebView2.ExecuteScriptAsync("window.player.snapshot()");
        public void Dispose() { if (!_disposed) { _disposed = true; _view.Dispose(); } }
    }
}
