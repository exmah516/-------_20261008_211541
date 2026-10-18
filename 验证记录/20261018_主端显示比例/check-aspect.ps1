param([ValidateSet('imaging','dsa')][string]$Mode)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName PresentationFramework, PresentationCore, WindowsBase, WindowsFormsIntegration, System.Windows.Forms, System.Drawing
$package = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$relative = if ($Mode -eq 'imaging') { '影像传输\A主端\工程\ImagingMaster' } else { '造影机器人\A主端\工程\DsaMaster' }
$name = if ($Mode -eq 'imaging') { 'ImagingMaster' } else { 'DsaMaster' }
$runtime = Join-Path $package ($relative + '\bin\Release\net472')
if ($Mode -eq 'dsa') {
    Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class VtkWindowProbe {
    public delegate bool EnumProc(IntPtr h, IntPtr p);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc callback, IntPtr p);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out Rect rect);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder name, int length);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int width, int height, uint flags);
    public static IntPtr Find(IntPtr parent) {
        IntPtr result = IntPtr.Zero;
        EnumChildWindows(parent, (h,p) => {
            var name = new StringBuilder(256); GetClassName(h, name, name.Capacity);
            if (name.ToString() == "vtkOpenGL") result = h;
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
'@
    $process = Start-Process -FilePath (Join-Path $runtime 'DsaMaster.exe') -WorkingDirectory $runtime -WindowStyle Hidden -PassThru
    try {
        Start-Sleep -Seconds 3; $process.Refresh()
        if ($process.HasExited -or !$process.Responding) { throw 'DSA startup failed' }
        $native = [VtkWindowProbe]::Find($process.MainWindowHandle)
        if ($native -eq [IntPtr]::Zero) { throw 'VTK renderer window missing' }
        $rect = New-Object VtkWindowProbe+Rect
        [VtkWindowProbe]::GetWindowRect($native, [ref]$rect) | Out-Null
        $ratio = ($rect.Right-$rect.Left)/[double]($rect.Bottom-$rect.Top)
        foreach($size in @(@(1280,500),@(640,900),@(400,600),@(900,700))) {
            if (![VtkWindowProbe]::SetWindowPos($process.MainWindowHandle,[IntPtr]::Zero,0,0,$size[0],$size[1],22)) { throw 'Resize failed' }
            Start-Sleep -Milliseconds 400
            [VtkWindowProbe]::GetWindowRect($native, [ref]$rect) | Out-Null
            $width=$rect.Right-$rect.Left; $height=$rect.Bottom-$rect.Top
            if ($height -le 0 -or [Math]::Abs($width-$height*$ratio) -gt 3) { throw "VTK aspect changed: $width x $height" }
            Write-Output "PASS: DSA window $($size[0]) x $($size[1]); native viewport $width x $height keeps aspect"
        }
        if (!$process.CloseMainWindow() -or !$process.WaitForExit(10000) -or $process.ExitCode -ne 0) { throw 'DSA did not close normally' }
        Write-Output 'PASS: DSA executable starts, resizes and exits normally'
    } finally { if (!$process.HasExited) { $process.Kill() } }
    exit 0
}
Set-Location -LiteralPath $runtime
[Environment]::CurrentDirectory = $runtime
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class NativeSearch {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] public static extern bool SetDllDirectory(string path);
}
'@
[NativeSearch]::SetDllDirectory($runtime) | Out-Null
$resolve = [ResolveEventHandler] { param($sender, $eventArgs)
    $path = Join-Path $runtime (($eventArgs.Name.Split(',')[0]) + '.dll')
    if (Test-Path -LiteralPath $path) { return [Reflection.Assembly]::LoadFrom($path) }
    return $null
}
[AppDomain]::CurrentDomain.add_AssemblyResolve($resolve)
$assembly = [Reflection.Assembly]::LoadFrom((Join-Path $runtime ($name + '.exe')))
[System.Windows.Application]::ResourceAssembly = $assembly
$app = [Activator]::CreateInstance($assembly.GetType('MasterConsole.App'))
$app.InitializeComponent()
$app.ShutdownMode = [System.Windows.ShutdownMode]::OnExplicitShutdown
$flags = [Reflection.BindingFlags]'Instance,NonPublic'
function Pump { $app.Dispatcher.Invoke([Action]{}, [System.Windows.Threading.DispatcherPriority]::ApplicationIdle) }
function Assert-Fit($hostControl, $area, [double]$ratio) {
    $w = $hostControl.ActualWidth; $h = $hostControl.ActualHeight
    if ($h -le 0 -or [Math]::Abs($w - $h * $ratio) -gt 2) { throw "Aspect changed: $w x $h, expected $ratio" }
    if ($w -gt $area.ActualWidth + 1 -or $h -gt $area.ActualHeight + 1) { throw 'Display exceeds its available area' }
    $offset = $hostControl.TranslatePoint([System.Windows.Point]::new(0,0), $area)
    if ([Math]::Abs($offset.X - ($area.ActualWidth-$w)/2) -gt 2 -or [Math]::Abs($offset.Y - ($area.ActualHeight-$h)/2) -gt 2) { throw 'Display is not centered' }
}
function Layout($control, $width, $height) {
    $control.Measure([System.Windows.Size]::new($width,$height))
    $control.Arrange([System.Windows.Rect]::new(0,0,$width,$height))
    $control.UpdateLayout(); Pump
}
$sizes = @(@(640,480),@(1280,360),@(360,900),@(320,360))
$window = $null; $view = $null
try {
    if ($Mode -eq 'imaging') {
        foreach ($sourceSize in @(@(1600,900),@(800,600),@(600,800))) {
            $view = [Activator]::CreateInstance($assembly.GetType('MasterConsole.Controls.DsaImageView'))
            $image = $view.FindName('Image')
            $w=$sourceSize[0]; $h=$sourceSize[1]
            $bitmap = [System.Windows.Media.Imaging.BitmapSource]::Create($w,$h,96,96,[System.Windows.Media.PixelFormats]::Bgr32,$null,(New-Object byte[] ($w*$h*4)),($w*4))
            $encoder = New-Object System.Windows.Media.Imaging.JpegBitmapEncoder
            $encoder.Frames.Add([System.Windows.Media.Imaging.BitmapFrame]::Create($bitmap))
            $stream = New-Object System.IO.MemoryStream
            $encoder.Save($stream)
            if (!$view.PublishJpeg($stream.ToArray(),[DateTime]::UtcNow)) { throw 'JPEG rejected' }
            $stream.Dispose()
            $deadline = [DateTime]::UtcNow.AddSeconds(5)
            while (!$image.Source -and [DateTime]::UtcNow -lt $deadline) { Pump; Start-Sleep -Milliseconds 25 }
            if (!$image.Source) { throw 'JPEG was not displayed' }
            foreach($size in $sizes) { Layout $view $size[0] $size[1]; Assert-Fit $image ([System.Windows.Media.VisualTreeHelper]::GetParent($image)) ($w/[double]$h) }
            $view.Dispose(); $view=$null
            Write-Output "PASS: DSA JPEG $w x $h keeps aspect at four window sizes"
        }
        $view = [Activator]::CreateInstance($assembly.GetType('MasterConsole.Controls.HikCameraView'))
        # 不触发 Loaded/摄像头登录，只验实际显示区域的缩放。
        $hostControl = $view.FindName('FormsHost'); $area = $view.FindName('VideoArea')
        $hostControl.Child = New-Object System.Windows.Forms.PictureBox
        if ($hostControl.Visibility -ne 'Hidden') { throw 'Preview should wait for decoded frame dimensions' }
        foreach($ratio in @((16.0/9),(4.0/3),(3.0/4))) {
            $view.GetType().GetField('_videoAspectRatio',$flags).SetValue($view,$ratio)
            foreach($size in $sizes) {
                Layout $view $size[0] $size[1]
                $view.GetType().GetMethod('UpdateVideoLayout',$flags).Invoke($view,@()) | Out-Null
                Layout $view $size[0] $size[1]
                Assert-Fit $hostControl $area $ratio
            }
        }
        Write-Output 'PASS: camera display fits and centers 16:9, 4:3 and portrait frames at four sizes'
        $hostControl.Dispose()
        $settings = [Activator]::CreateInstance($assembly.GetType('MasterConsole.Services.HikCameraSettings'),$true)
        $session = [Activator]::CreateInstance($assembly.GetType('MasterConsole.Services.HikCameraSession'),$flags,$null,@($settings),$null)
        $dimensions = [object[]]@(0,0)
        if ($session.GetType().GetMethod('TryGetVideoSize',$flags).Invoke($session,$dimensions)) { throw 'Disconnected camera returned frame dimensions' }
        $session.Dispose()
        Write-Output 'PASS: no frame dimensions are invented for disconnected cameras'
    }
} finally {
    if ($view) { $view.Dispose() }
    if ($window) { $window.Close() }
    $app.Shutdown()
    [AppDomain]::CurrentDomain.remove_AssemblyResolve($resolve)
    [NativeSearch]::SetDllDirectory($null) | Out-Null
}
Write-Output 'PASS: controls closed normally'
exit 0
