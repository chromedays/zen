git clean -fdX
premake5 vs2019
$compress = @{
    Path = ".\build", ".\data", ".\dep", ".\src", ".\temp", ".\.clang-format", ".\zen.sln", ".\zen_win32.*"
    CompressionLevel = "Optimal"
    DestinationPath = ".\zen.zip"
}
Compress-Archive @compress
