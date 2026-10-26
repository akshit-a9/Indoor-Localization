#!/usr/bin/env pwsh
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent $ScriptDir

$VenvActivate = Join-Path $RepoRoot ".venv\Scripts\Activate.ps1"
if (Test-Path $VenvActivate) {
    . $VenvActivate
} else {
    Write-Error "No virtualenv found at $RepoRoot\.venv"
    exit 1
}

Set-Location $ScriptDir
python -m uvicorn app.main:app --host 0.0.0.0 --port 8000
