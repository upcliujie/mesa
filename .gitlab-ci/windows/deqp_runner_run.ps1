$dxil_dll = cmd.exe /C "C:\BuildTools\Common7\Tools\VsDevCmd.bat -host_arch=amd64 -arch=amd64 -no_logo && where dxil.dll" 2>&1
if ($dxil_dll -notmatch "dxil.dll$") {
    Write-Output "Couldn't get path to dxil.dll"
    exit 1
}
$env:Path = "$(Split-Path $dxil_dll);$env:Path"

# VK_ICD_FILENAMES environment variable is not used when running with
# elevated privileges. Add a key to the registry instead.
$hkey_path = "HKLM:\SOFTWARE\Khronos\Vulkan\Drivers\"
$hkey_name = Join-Path -Path $pwd -ChildPath "_install\share\vulkan\icd.d\dzn_icd.x86_64.json"
New-Item -Path $hkey_path -force
New-ItemProperty -Path $hkey_path -Name $hkey_name -Value 0 -PropertyType DWORD

$results = New-Item -ItemType Directory results

# Set default values if needed
if ($env:DEQP_WIDTH -eq $null) {
    $env:DEQP_WIDTH = 256
}
if ($env:DEQP_HEIGHT -eq $null) {
    $env:DEQP_HEIGHT = 256
}
if ($env:DEQP_SURFACE_TYPE -eq $null) {
    $env:DEQP_SURFACE_TYPE = "pbuffer"
}
if ($env:DEQP_CONFIG -eq $null) {
    $env:DEQP_CONFIG = "rgba8888d24s8ms0"
}
if ($env:DEQP_FRACTION -eq $null) {
    $env:DEQP_FRACTION = 1
}
if ($env:CI_NODE_INDEX -eq $null) {
    $env:CI_NODE_INDEX = 1
}
if ($env:FDO_CI_CONCURRENT -eq $null) {
    $env:FDO_CI_CONCURRENT = 4
}

# Adjust fraction if CI parallel mode is enabled
if ($env:CI_NODE_TOTAL -ne $null) {
    $env:DEQP_FRACTION *= $env:CI_NODE_TOTAL
}

$deqp_options = @(
    "--deqp-surface-width", $env:DEQP_WIDTH,
    "--deqp-surface-height", $env:DEQP_HEIGHT,
    "--deqp-surface-type", $env:DEQP_SURFACE_TYPE,
    "--deqp-gl-config-name", $env:DEQP_CONFIG,
    "--deqp-visibility", "hidden"
)
$deqp_runner_options = @(
    "--deqp", "C:\deqp\external\vulkancts\modules\vulkan\deqp-vk.exe",
    "--output", $results.FullName,
    "--caselist", "C:\deqp\mustpass\vk-master.txt",
    "--baseline", ".\_install\dozen-warp-fails.txt",
    "--flakes", ".\_install\dozen-warp-flakes.txt",
    "--include-tests", "dEQP-VK.api.*",
    "--include-tests", "dEQP-VK.info.*",
    "--include-tests", "dEQP-VK.draw.*",
    "--include-tests", "dEQP-VK.query_pool.*",
    "--include-tests", "dEQP-VK.memory.*",
    "--testlog-to-xml", "C:\deqp\executor\testlog-to-xml.exe",
    "--jobs", $env:FDO_CI_CONCURRENT,
    "--fraction-start", $env:CI_NODE_INDEX,
    "--fraction", $env:DEQP_FRACTION
)

$env:DZN_DEBUG = "warp"
$env:MESA_VK_IGNORE_CONFORMANCE_WARNING = "true"
deqp-runner run $($deqp_runner_options) -- $($deqp_options)
$deqpstatus = $?

$template = "See https://$($env:CI_PROJECT_ROOT_NAMESPACE).pages.freedesktop.org/-/$($env:CI_PROJECT_NAME)/-/jobs/$($env:CI_JOB_ID)/artifacts/results/{{testcase}}.xml"
deqp-runner junit --testsuite dEQP --results "$($results)/failures.csv" --output "$($results)/junit.xml" --limit 50 --template $template

if (!$deqpstatus) {
    Exit 1
}
