@echo off
pushd "B:\dev\Prototypes\netcode\build"
REM Optimization switches /O2 /Oi /fp:fast
REM set CommonCompilerFlags=/O2 /MTd /nologo /fp:fast /Gm- /GR- /EHa /Zo /Oi /WX /W4 /wd4201 /wd4100 /wd4189 /wd4505 /wd4127 /DHANDMADE_INTERNAL=1 /DHANDMADE_SLOW=1 /DHANDMADE_WIN32 /Z7 /FC /F4194304
REM set CommonLinkerFlags= -incremental:no -opt:ref  user32.lib gdi32.lib winmm.lib
SET CommonCompilerFlags= /MP /GR- /Zi /Zo /Oi /WX /EHsc -W3 -wd4201 -wd4100
@rem SET CommonCompilerFlags= -O2 -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -wd4127 -FC -Z7 -GS- -Gs9999999
set Includes= /I "../" /I "B:/frameworks/SDL2-2.28.4/include" /I "B:/frameworks/SDL2_ttf-2.20.2/include" /I "B:\frameworks\SDL2_mixer-2.8.0\include" /I "B:\frameworks\SDL2_image-2.6.3\include"
set Libraries= SDL2main.lib SDL2.lib SDL2_image.lib SDL2_ttf.lib SDL2_mixer.lib
set LinkerFlags= -incremental:no winmm.lib user32.lib gdi32.lib ws2_32.lib %Libraries%
@REM set ImguiSources= "../vendor/imgui/imgui.cpp" "../vendor/imgui/imgui_draw.cpp" "../vendor/imgui/imgui_tables.cpp" "../vendor/imgui/imgui_widgets.cpp" "../vendor/imgui/backends/imgui_impl_sdl2.cpp" "../vendor/imgui/backends/imgui_impl_sdlrenderer2.cpp" "../vendor/imgui/misc/cpp/imgui_stdlib.cpp"
@REM %date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%
@REM cl %CommonCompilerFlags% %ImguiSources% %Includes% -LD /link %LinkerFlags%
@REM del game_*.pdb
@REM set PDB=game_%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%.pdb
@REM /PDB:game_%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%.pdb
@REM /PDB:game_%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%
cl %CommonCompilerFlags% /Fe: main.exe "../src/Application.cpp" %Includes% /link %LinkerFlags%
popd
