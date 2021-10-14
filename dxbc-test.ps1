param(
        [Parameter(Mandatory)]
        [String]
        $dxc_with_spirv_support_path,

        # This is a program that is a trivial wrapper around https://www.nuget.org/packages/Microsoft.Direct3D.DxbcSigner.
        [String]
        $dxbc_signer_path,

        [String]
        $fxc_path = "fxc"
)

$build_dir = "build\debug"
$vs_entry_name = "VSMain"
$ps_entry_name = "PSMain"

if (-not (Test-Path $build_dir)) {
        & meson setup --buildtype=debug -Dopengl=false -Dgles1=disabled -Dgles2=disabled -Ddri-drivers= -Dgallium-drivers= -Dvulkan-drivers= -Dzlib=disabled -Dzstd=disabled -Dshader-cache=disabled -Dspirv-to-dxil=true $build_dir
}

& meson compile -C $build_dir src/microsoft/spirv_to_dxil/spirv2dxbc
if ($LASTEXITCODE) {
        exit 1
}

$spirv2dxbc_path = "$build_dir\src\microsoft\spirv_to_dxil\spirv2dxbc.exe"

$shader_dir = "src\microsoft\spirv_to_dxil\test_shaders"
$shader_output_dir = "$build_dir\_shaders"
New-Item $shader_output_dir -ErrorAction SilentlyContinue

$files = @(Get-ChildItem "$shader_dir\*.hlsl")
foreach ($file in $files) {
        Write-Host -ForegroundColor Green "compiling $file"
        $file_name = $file.Name;

        # Dump DXBC assembly listing from FXC directly
        Write-Host -ForegroundColor Green "...to dxbc via fxc"
        & $fxc_path /nologo /T vs_5_1 /E $vs_entry_name $file -Fo "$shader_output_dir\$file_name.vs.fxc.dxbc"
        if ($LASTEXITCODE) {
                Write-Error "compiling vs via fxc failed"
                exit 1
        }
        & $fxc_path /nologo /T ps_5_1 /E $ps_entry_name $file -Fo "$shader_output_dir\$file_name.ps.fxc.dxbc"
        if ($LASTEXITCODE) {
                Write-Error "compiling ps via fxc failed"
                exit 1
        }
        & $fxc_path /nologo /dumpbin "$shader_output_dir\$file_name.vs.fxc.dxbc" -Fc "$shader_output_dir\$file_name.vs.fxc.disasm"
        & $fxc_path /nologo /dumpbin "$shader_output_dir\$file_name.ps.fxc.dxbc" -Fc "$shader_output_dir\$file_name.ps.fxc.disasm"


        # Compile HLSL to DXBC via SPIR-V using spirv2dxbc
        Write-Host -ForegroundColor Green "...to spir-v"
        & $dxc_with_spirv_support_path -nologo -T vs_6_0 -E $vs_entry_name -spirv $file -Fo "$shader_output_dir\$file_name.vs.spv"
        & $dxc_with_spirv_support_path -nologo -T ps_6_0 -E $ps_entry_name -spirv $file -Fo "$shader_output_dir\$file_name.ps.spv"
        Write-Host -ForegroundColor Green "...to dxbc via mesa"
        Write-Host -ForegroundColor Yellow "$spirv2dxbc_path $shader_output_dir\$file_name.vs.spv -s vertex -e $vs_entry_name -o $shader_output_dir\$file_name.vs.mesa.dxbc"
        & $spirv2dxbc_path "$shader_output_dir\$file_name.vs.spv" -s vertex -e $vs_entry_name -o "$shader_output_dir\$file_name.vs.mesa.dxbc"
        if ($LASTEXITCODE) {
                Write-Error "compiling vs via mesa failed"
                exit 1
        }
        Write-Host -ForegroundColor Yellow "$spirv2dxbc_path $shader_output_dir\$file_name.ps.spv -s fragment -e $ps_entry_name -o $shader_output_dir\$file_name.ps.mesa.dxbc"
        & $spirv2dxbc_path "$shader_output_dir\$file_name.ps.spv" -s fragment -e $ps_entry_name -o "$shader_output_dir\$file_name.ps.mesa.dxbc"
        if ($LASTEXITCODE) {
                Write-Error "compiling ps via mesa failed"
                exit 1
        }

        if ($dxbc_signer_path) {
                Write-Host -ForegroundColor Green "...signing dxbc"
                & $dxbc_signer_path "$shader_output_dir\$file_name.vs.mesa.dxbc" "$shader_output_dir\$file_name.vs.mesa.dxbc"
                & $dxbc_signer_path "$shader_output_dir\$file_name.ps.mesa.dxbc" "$shader_output_dir\$file_name.ps.mesa.dxbc"
                & $fxc_path /nologo /dumpbin "$shader_output_dir\$file_name.vs.mesa.dxbc" -Fc "$shader_output_dir\$file_name.vs.mesa.disasm"
                & $fxc_path /nologo /dumpbin "$shader_output_dir\$file_name.ps.mesa.dxbc" -Fc "$shader_output_dir\$file_name.ps.mesa.disasm"
        } else {
                Write-Host -ForegroundColor Red "need dxbcsigner to dump disassembly!"
        }
}
