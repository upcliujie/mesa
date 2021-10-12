param(
        [Parameter(Mandatory)]
        [String]
        $dxc_path,

        [String]
        $fxc_path = "fxc",

        [String]
        $build_dir = "build\debug"
)

$vs_entry_name = "VSMain"
$ps_entry_name = "PSMain"

& meson compile -C $build_dir src/microsoft/spirv_to_dxil/spirv2dxbc
if ($LASTEXITCODE) {
        exit 1
}

$spirv2dxbc_path = "$build_dir\src\microsoft\spirv_to_dxil\spirv2dxbc.exe"

$shader_dir = "src\microsoft\spirv_to_dxil\test_shaders"
$shader_output_dir = "$build_dir\_shaders"

$files = @(Get-ChildItem "$shader_dir\*.hlsl")
foreach ($file in $files) {
        Write-Host "compiling $file"
        $file_name = $file.Name;

        # Compile HLSL to DXBC via SPIR-V using spirv2dxbc
        & $dxc_path -T vs_6_0 -E $vs_entry_name -spirv $file -Fo "$shader_output_dir\$file_name.vs.spv"
        & $dxc_path -T ps_6_0 -E $ps_entry_name -spirv $file -Fo "$shader_output_dir\$file_name.ps.spv"
        & $spirv2dxbc_path "$shader_output_dir\$file_name.vs.spv" -s vertex -e $vs_entry_name -o "$shader_output_dir\$file_name.vs.mesa.dxbc"
        if ($LASTEXITCODE) {
                Write-Error "compiling shader_dir\$file_name.vs.spv failed"
                exit 1
        }
        & $spirv2dxbc_path "$shader_output_dir\$file_name.ps.spv" -s fragment -e $ps_entry_name -o "$shader_output_dir\$file_name.ps.mesa.dxbc"
        if ($LASTEXITCODE) {
                Write-Error "compiling shader_dir\$file_name.ps.spv failed"
                exit 1
        }

        # Dump DXBC assembly listing for outputted shaders
        & $fxc_path /dumpbin "$shader_output_dir\$file_name.vs.mesa.dxbc" -Fc "$shader_output_dir\$file_name.vs.mesa.disasm"
        & $fxc_path /dumpbin "$shader_output_dir\$file_name.ps.mesa.dxbc" -Fc "$shader_output_dir\$file_name.ps.mesa.disasm"

        # Dump DXBC assembly listing from FXC directly
        & $fxc_path /T vs_5_1 /E $vs_entry_name $file -Fc "$shader_output_dir\$file_name.vs.fxc.disasm"
        & $fxc_path /T ps_5_1 /E $ps_entry_name $file -Fc "$shader_output_dir\$file_name.ps.fxc.disasm"
}
