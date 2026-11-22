param([int]$Seconds=120, [string]$Output='LiteReplay-metrics.csv')
$ErrorActionPreference='Stop'
if($Seconds -lt 1 -or $Seconds -gt 86400){throw 'Seconds must be between 1 and 86400'}
$target = Get-Process LiteReplay -ErrorAction Stop | Select-Object -First 1
$processId = $target.Id
$cores = [Environment]::ProcessorCount
$lastCpu = $target.TotalProcessorTime.TotalSeconds
$watch = [Diagnostics.Stopwatch]::StartNew()
$lastTime = 0.0
$rows = for($i=0;$i -lt $Seconds;$i++) {
    Start-Sleep -Seconds 1
    $target.Refresh()
    if($target.HasExited){break}
    $now=$watch.Elapsed.TotalSeconds
    $cpu=$target.TotalProcessorTime.TotalSeconds
    [pscustomobject]@{
        Time=(Get-Date -Format o)
        ProcessId=$processId
        CPUPercent=100*($cpu-$lastCpu)/($now-$lastTime)/$cores
        PrivateMiB=$target.PrivateMemorySize64/1MB
        WorkingSetMiB=$target.WorkingSet64/1MB
        Handles=$target.HandleCount
        Threads=$target.Threads.Count
    }
    $lastCpu=$cpu;$lastTime=$now
}
$rows | Export-Csv -NoTypeInformation -Encoding UTF8 $Output
Write-Host "Wrote measured samples to $Output. Record game FPS/frametime and GPU Video Encode separately."
