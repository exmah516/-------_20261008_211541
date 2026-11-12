param([string]$LogDirectory = (Join-Path $PSScriptRoot '../docs/refactor_stage0/logs/plc'))
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$logDir = [IO.Path]::GetFullPath($LogDirectory)
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$log = Join-Path $logDir 'build.log'
$work = Join-Path ([IO.Path]::GetTempPath()) ('plc-build-' + [Guid]::NewGuid().ToString('N'))
$result = [ordered]@{ success = $false; exit_code = 1; errors = $null; warnings = $null; log = $log; workspace = $work; interface = 'TcXaeShell.DTE.15.0 / SolutionBuild.BuildProject' }
$dte = $null
$lines = [Collections.Generic.List[string]]::new()
try {
    # XAE 只打开副本并执行 BuildProject；不调用在线、下载、激活或目标系统接口。
    Copy-Item -LiteralPath (Join-Path $root '介入机器人/C从端') -Destination $work -Recurse
    $dte = New-Object -ComObject TcXaeShell.DTE.15.0
    $dte.SuppressUI = $true
    $dte.MainWindow.Visible = $false
    $dte.Solution.Open((Join-Path $work '250902.sln'))
    $project = $null
    foreach ($candidate in $dte.Solution.Projects) {
        $lines.Add(('Project: {0}; Kind: {1}; File: {2}' -f $candidate.Name, $candidate.Kind, $candidate.FullName))
        if ($candidate.Name -eq '250902') { $project = $candidate }
    }
    if (!$project -or !$project.FullName -or $project.Kind -eq '{67294A52-A4F0-11D2-AA88-00C04F688DDE}') {
        throw 'TwinCAT project did not load: XAE returned an unmodeled/unavailable project. No PLC compilation was performed.'
    }
    $config = 'Release|TwinCAT RT (x64)'
    $lines.Add(('BuildProject: {0}; {1}' -f $config, $project.UniqueName))
    $dte.Solution.SolutionBuild.BuildProject($config, $project.UniqueName, $true)
    $result['last_build_info'] = $dte.Solution.SolutionBuild.LastBuildInfo
    $result.errors = 0
    $result.warnings = 0
    $items = $dte.ToolWindows.ErrorList.ErrorItems
    for ($i = 1; $i -le $items.Count; $i++) {
        $item = $items.Item($i)
        $lines.Add(('{0}:{1} level={2} {3}' -f $item.FileName, $item.Line, $item.ErrorLevel, $item.Description))
        if ($item.ErrorLevel -eq 2) { $result.errors++ }
        if ($item.ErrorLevel -eq 1) { $result.warnings++ }
    }
    $result.success = $result.last_build_info -eq 0 -and $result.errors -eq 0
    if ($result.success) { $result.exit_code = 0 }
} catch {
    $result['failure'] = $_.Exception.Message
    $lines.Add($_.Exception.ToString())
} finally {
    if ($dte) {
        foreach ($pane in $dte.ToolWindows.OutputWindow.OutputWindowPanes) {
            $doc = $pane.TextDocument
            $point = $doc.StartPoint.CreateEditPoint()
            $lines.Add(('--- {0} ---' -f $pane.Name))
            $lines.Add($point.GetText($doc.EndPoint))
        }
        $dte.Solution.Close($false)
        $dte.Quit()
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($dte)
    }
    [IO.File]::WriteAllLines($log, $lines, [Text.UTF8Encoding]::new($true))
    $result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $logDir 'result.json') -Encoding UTF8
    $lines | Write-Output
}
exit $result.exit_code
