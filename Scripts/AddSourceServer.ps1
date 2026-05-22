param(
    [string]$PdbPath = "",
    [string]$BuildDir = "",
    [string]$RepoRoot = (Join-Path $PSScriptRoot ".."),
    [string]$SourceRepo = "\\172.21.11.100\SourceRepos\Week12.git",
    [string]$Commit = "",
    [string]$SrcToolPath = "",
    [string]$PdbStrPath = "",
    [switch]$KeepStreamFiles
)

$ErrorActionPreference = "Stop"

function Resolve-DebuggingTool {
    param(
        [string]$ToolName,
        [string]$ExplicitPath
    )

    if ($ExplicitPath) {
        if (Test-Path -LiteralPath $ExplicitPath) {
            return (Resolve-Path -LiteralPath $ExplicitPath).Path
        }

        throw "$ToolName not found at '$ExplicitPath'."
    }

    $fromPath = Get-Command $ToolName -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    $sdkCandidates = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64\$ToolName",
        "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x86\$ToolName",
        "${env:ProgramFiles}\Windows Kits\10\Debuggers\x64\$ToolName"
    )

    foreach ($candidate in $sdkCandidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return $candidate
        }
    }

    throw "$ToolName not found. Install Windows SDK Debugging Tools or pass the explicit tool path."
}

function Convert-ToGitPath {
    param(
        [string]$Path,
        [string]$Root
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $fullRoot = [System.IO.Path]::GetFullPath($Root)

    if (-not $fullRoot.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $fullRoot += [System.IO.Path]::DirectorySeparatorChar
    }

    if (-not $fullPath.StartsWith($fullRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $null
    }

    return $fullPath.Substring($fullRoot.Length).Replace("\", "/")
}

function Write-SourceServerStream {
    param(
        [string]$StreamPath,
        [string]$PdbFile,
        [string[]]$SourceFiles,
        [string]$Root,
        [string]$GitRepo,
        [string]$GitCommit
    )

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("SRCSRV: ini ------------------------------------------------")
    $lines.Add("VERSION=2")
    $lines.Add("INDEXVERSION=2")
    $lines.Add("VERCTRL=Git")
    $lines.Add("DATETIME=$(Get-Date -Format o)")
    $lines.Add("SRCSRV: variables ------------------------------------------")
    $lines.Add("GIT_EXE=git.exe")
    $lines.Add("GIT_REPO=$GitRepo")
    $lines.Add("SRCSRVTRG=%targ%\%fnfile%(%var2%)")
    $lines.Add('SRCSRVCMD=cmd /c "%GIT_EXE%" --git-dir="%GIT_REPO%" show %var3%:%var2% > "%SRCSRVTRG%"')
    $lines.Add("SRCSRV: source files ---------------------------------------")

    $mappedCount = 0
    foreach ($sourceFile in $SourceFiles) {
        $relativePath = Convert-ToGitPath -Path $sourceFile -Root $Root
        if (-not $relativePath) {
            continue
        }

        $lines.Add("$sourceFile*$relativePath*$GitCommit")
        $mappedCount++
    }

    $lines.Add("SRCSRV: end ------------------------------------------------")
    [System.IO.File]::WriteAllLines($StreamPath, $lines, [System.Text.Encoding]::ASCII)

    if ($mappedCount -eq 0) {
        throw "No source files in '$PdbFile' were under repo root '$Root'."
    }

    return $mappedCount
}

$resolvedRepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path

try {
    $sourceRepoExists = Test-Path -LiteralPath $SourceRepo
} catch {
    throw "Cannot access source repo '$SourceRepo'. Check network share credentials and read permission. Detail: $($_.Exception.Message)"
}

if (-not $sourceRepoExists) {
    throw "Source repo not found: '$SourceRepo'. Check network share path and permissions."
}

if (-not $Commit) {
    $Commit = (& git -C $resolvedRepoRoot rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or -not $Commit) {
        throw "Failed to resolve git commit from '$resolvedRepoRoot'. Pass -Commit explicitly."
    }
}

if ($PdbPath) {
    $pdbFiles = @((Resolve-Path -LiteralPath $PdbPath).Path)
} elseif ($BuildDir) {
    $resolvedBuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
    $pdbFiles = @(Get-ChildItem -LiteralPath $resolvedBuildDir -Recurse -Filter "*.pdb" -File | ForEach-Object { $_.FullName })
} else {
    throw "Pass either -PdbPath or -BuildDir."
}

if (-not $pdbFiles) {
    throw "No PDB files found."
}

$srctool = Resolve-DebuggingTool -ToolName "srctool.exe" -ExplicitPath $SrcToolPath
$pdbstr = Resolve-DebuggingTool -ToolName "pdbstr.exe" -ExplicitPath $PdbStrPath

Write-Host "SrcTool   : $srctool"
Write-Host "PdbStr    : $pdbstr"
Write-Host "RepoRoot  : $resolvedRepoRoot"
Write-Host "SourceRepo: $SourceRepo"
Write-Host "Commit    : $Commit"
Write-Host "PDB Count : $($pdbFiles.Count)"

foreach ($pdbFile in $pdbFiles) {
    Write-Host "Processing: $pdbFile"

    $sourceFiles = @(& $srctool $pdbFile | Where-Object { $_ -match "^[A-Za-z]:\\|^\\\\" } | Sort-Object -Unique)
    if ($LASTEXITCODE -ne 0) {
        throw "srctool.exe failed for '$pdbFile' with exit code $LASTEXITCODE."
    }

    $streamPath = Join-Path ([System.IO.Path]::GetTempPath()) ("srcsrv_{0}.txt" -f ([System.Guid]::NewGuid().ToString("N")))
    $mappedCount = Write-SourceServerStream -StreamPath $streamPath -PdbFile $pdbFile -SourceFiles $sourceFiles -Root $resolvedRepoRoot -GitRepo $SourceRepo -GitCommit $Commit

    & $pdbstr -w "-p:$pdbFile" "-i:$streamPath" -s:srcsrv
    if ($LASTEXITCODE -ne 0) {
        throw "pdbstr.exe failed for '$pdbFile' with exit code $LASTEXITCODE."
    }

    Write-Host "Embedded source server stream. Source Count: $mappedCount"

    if (-not $KeepStreamFiles) {
        Remove-Item -LiteralPath $streamPath -Force
    } else {
        Write-Host "Stream file kept: $streamPath"
    }
}

Write-Host "Source server data embedded successfully."
