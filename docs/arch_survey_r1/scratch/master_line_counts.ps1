$ErrorActionPreference = 'Stop'
$surveyRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$masterRoot = Join-Path $surveyRoot '介入机器人/A主端/工程/master'
$rows = Get-ChildItem -LiteralPath $masterRoot -Recurse -File -Include *.cs,*.xaml |
    Where-Object { $_.FullName -notmatch '[\\/](bin|obj)[\\/]' } |
    ForEach-Object {
        [pscustomobject]@{
            File = $_.FullName.Substring($masterRoot.Length + 1).Replace('\', '/')
            PhysicalLines = [System.IO.File]::ReadAllLines($_.FullName).Length
        }
    } | Sort-Object -Property @{ Expression = 'PhysicalLines'; Descending = $true }, File
$rows | Export-Csv -LiteralPath (Join-Path $PSScriptRoot 'master_line_counts.csv') -NoTypeInformation -Encoding utf8
$rows | Select-Object -First 30 | Format-Table -AutoSize
