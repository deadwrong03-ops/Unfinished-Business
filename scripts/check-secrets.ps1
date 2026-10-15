$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot

$excludedDirs = @(
    ".git", ".vs", "x64", "Debug", "Release", "build", "out", "dist"
)

$excludedExts = @(
    ".dll", ".exe", ".obj", ".pdb", ".lib", ".exp", ".ilk",
    ".png", ".jpg", ".jpeg", ".gif", ".zip", ".7z", ".bin"
)

$patterns = @(
    @{
        Name = "GW2 API key"
        Regex = '(?i)\b[A-F0-9]{8}(?:-[A-F0-9]{4}){3}-[A-F0-9]{20}-[A-F0-9]{4}-[A-F0-9]{12}\b'
    },
    @{
        Name = "GitHub token"
        Regex = '\bgh[pousr]_[A-Za-z0-9_]{20,}\b'
    },
    @{
        Name = "AWS access key"
        Regex = '\bAKIA[0-9A-Z]{16}\b'
    },
    @{
        Name = "Private key"
        Regex = '-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----'
    },
    @{
        Name = "Literal bearer token"
        Regex = '(?i)Authorization\s*:\s*Bearer\s+[A-Za-z0-9._~+/=-]{16,}'
    },
    @{
        Name = "Likely hard-coded secret"
        Regex = '(?i)(api[_ -]?key|token|secret|password)\s*[:=]\s*["''][^"'']{16,}["'']'
    }
)

$findings = @()

Get-ChildItem -Path $root -Recurse -File | ForEach-Object {
    $file = $_

    # PowerShell 5.1-safe relative path handling.
    $relative = $file.FullName.Substring($root.Length).TrimStart([char]92, [char]47)
    $parts = $relative -split '[\\/]'

    if ($parts | Where-Object { $excludedDirs -contains $_ }) {
        return
    }

    if ($excludedExts -contains $file.Extension.ToLowerInvariant()) {
        return
    }

    try {
        $content = Get-Content -LiteralPath $file.FullName -Raw -ErrorAction Stop
    }
    catch {
        return
    }

    foreach ($pattern in $patterns) {
        if ($content -match $pattern.Regex) {
            $findings += [PSCustomObject]@{
                File = $relative
                Type = $pattern.Name
            }
        }
    }
}

# Scan existing Git history only when the repo already has commits.
$hasHead = $false
try {
    & git -C $root rev-parse --verify HEAD *> $null
    if ($LASTEXITCODE -eq 0) {
        $hasHead = $true
    }
}
catch {
    $hasHead = $false
}

if ($hasHead) {
    try {
        $history = (& git -C $root log -p --all --no-color 2>$null) -join "`n"

        foreach ($pattern in $patterns) {
            if ($history -match $pattern.Regex) {
                $findings += [PSCustomObject]@{
                    File = "[Git history]"
                    Type = $pattern.Name
                }
            }
        }
    }
    catch {
        Write-Host "Warning: Git history scan could not be completed." -ForegroundColor Yellow
    }
}

if ($findings.Count -gt 0) {
    Write-Host ""
    Write-Host "Possible secret material found:" -ForegroundColor Red
    $findings |
        Sort-Object File, Type -Unique |
        Format-Table -AutoSize

    Write-Host ""
    Write-Host "Do NOT commit or push until every finding is reviewed." -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Secret scan passed. No known credential patterns found." -ForegroundColor Green
exit 0
