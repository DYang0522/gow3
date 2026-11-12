# Offline Bloodborne AppImage package; build.sh supplies the already-built binaries.
{ pkgs ? import <nixpkgs> { }
  # RUNPATH store references gathered by bloodborne-appimage.sh.
, runtimePaths ? (if builtins.pathExists ./runtime-paths-bloodborne.nix
                   then import ./runtime-paths-bloodborne.nix else [ ])
}:
let
  lib = pkgs.lib;
  root = ./..;
  assetDirs = [ "port/bloodborne/scripts" "port/bloodborne/patches" ];
  launcherFiles = [ "launcher/bbport_launcher.py" "launcher/bbport_i18n.py" ];
  wanted = [
    "port/bloodborne/run.sh" "port/bloodborne/run.py"
    "out/bb-probe" "out/bb-gpu-capabilities" "out/gpu/libbbgpu.so"
  ] ++ launcherFiles;
  src = builtins.path {
    name = "bloodborne-port-src";
    path = root;
    filter = path: type:
      let rel = lib.removePrefix (toString root + "/") (toString path);
      in builtins.elem rel wanted
        || lib.any (dir: rel == dir || lib.hasPrefix (dir + "/") rel) assetDirs
        || lib.any (file: lib.hasPrefix (rel + "/") file) wanted;
  };
  # The Linux launcher decodes the linked executable to build its CPU-instruction rewrite table.
  # CPython's stable-ABI wheel supports the bundled Python 3.x without compiling Rust in the package.
  icedX86 = pkgs.python3Packages.buildPythonPackage {
    pname = "iced-x86";
    version = "1.21.0";
    format = "wheel";
    src = pkgs.fetchurl {
      url = "https://files.pythonhosted.org/packages/5d/08/054cac5c4f435a243e423323783992c78c7c2f22cf2269fcca145b8423af/iced_x86-1.21.0-cp38-abi3-manylinux_2_17_x86_64.manylinux2014_x86_64.whl";
      hash = "sha256-SJeJN3v8H+QyvSqKV4DJ8+fDW+LmEzsN5UMs2QHK0yw=";
    };
    nativeBuildInputs = [ pkgs.autoPatchelfHook ];
    buildInputs = [ pkgs.stdenv.cc.cc.lib ];
    doCheck = false;
  };
  python = pkgs.python3.withPackages (ps: [ ps.pygobject3 ps.pillow icedX86 ]);
  # This AppImage targets the user's GCN R9 290X: bundle RADV and RadeonSI only,
  # not every Mesa Vulkan/Gallium driver (Intel, Nouveau, Lavapipe, etc.).
  mesa = pkgs.mesa.override {
    galliumDrivers = [ "radeonsi" ];
    vulkanDrivers = [ "amd" ];
    vulkanLayers = [ ];
  };
  icds = "${mesa}/share/vulkan/icd.d/radeon_icd.x86_64.json";
  fontsConf = pkgs.writeText "bbport-fonts.conf" ''
    <?xml version="1.0"?>
    <!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
    <fontconfig>
      <dir>${pkgs.dejavu_fonts}/share/fonts</dir>
      <dir>${pkgs.adwaita-fonts}/share/fonts</dir>
      <dir>/usr/share/fonts</dir>
      <dir>/usr/local/share/fonts</dir>
      <dir prefix="xdg">fonts</dir>
      <cachedir prefix="xdg">bbport/fontconfig</cachedir>
      <include ignore_missing="yes">${pkgs.fontconfig.out}/etc/fonts/conf.d</include>
    </fontconfig>
  '';
  common = ''
      --prefix XDG_DATA_DIRS : ${pkgs.mangohud}/share:${pkgs.adwaita-icon-theme}/share:${pkgs.hicolor-icon-theme}/share:${pkgs.gtk4}/share/gsettings-schemas/${pkgs.gtk4.name} \
      --set-default GDK_PIXBUF_MODULE_FILE ${pkgs.librsvg}/${pkgs.gdk-pixbuf.moduleDir}.cache \
      --set-default FONTCONFIG_FILE ${fontsConf} \
      --set-default LC_ALL C.UTF-8 \
      --set-default VK_DRIVER_FILES ${icds} \
      --set-default VK_ICD_FILENAMES ${icds} \
      --prefix LD_LIBRARY_PATH : ${lib.makeLibraryPath [ pkgs.libglvnd pkgs.libx11 pkgs.libxext ]} \
  '';
in
pkgs.stdenv.mkDerivation {
  pname = "bbport";
  version = "0.1";
  inherit src;
  nativeBuildInputs = [ pkgs.makeShellWrapper pkgs.wrapGAppsHook4 pkgs.gobject-introspection ];
  buildInputs = [ pkgs.gtk4 pkgs.libadwaita pkgs.adwaita-icon-theme pkgs.librsvg
                  mesa pkgs.vulkan-loader ] ++ map builtins.storePath runtimePaths;
  dontBuild = true;
  dontConfigure = true;
  # Build.sh uses -g for diagnostics; strip debug sections from every packaged runtime ELF.
  stripDebugList = [ "share/bbport/bin" ];
  dontPatchELF = true;
  dontWrapGApps = true;
  installPhase = ''
    runHook preInstall
    d=$out/share/bbport
    mkdir -p $d/bin/gpu $d/launcher $out/bin $out/share/applications
    cp port/bloodborne/run.sh port/bloodborne/run.py $d/
    cp -r port/bloodborne/scripts port/bloodborne/patches $d/
    cp launcher/bbport_launcher.py launcher/bbport_i18n.py $d/launcher/
    install -m755 out/bb-probe $d/bin/bb-probe
    install -m755 out/bb-gpu-capabilities $d/bin/bb-gpu-capabilities
    install -m755 out/gpu/libbbgpu.so $d/bin/gpu/libbbgpu.so
    install -m644 ${./bloodborne.desktop} $out/share/applications/bbport.desktop
    runHook postInstall
  '';
  postFixup = ''
    mkdir -p $out/libexec
    gcc -O2 -Wall -Werror -static -L${pkgs.glibc.static}/lib -DTARGET="\"$out/libexec/bbport\"" \
      ${./bbport-entry.c} -o $out/bin/bbport
    makeShellWrapper ${python}/bin/python3 $out/libexec/bbport \
      "''${gappsWrapperArgs[@]}" \
      ${common} \
      --add-flags "$out/share/bbport/launcher/bbport_launcher.py" \
      --set BB_PREBUILT 1 \
      --set PYTHON ${python}/bin/python3 \
      --prefix PATH : ${lib.makeBinPath [ pkgs.bash pkgs.coreutils pkgs.util-linux ]} \
      --run 'export BB_DATA_DIR=''${BB_DATA_DIR:-''${XDG_DATA_HOME:-$HOME/.local/share}/bbport}; mkdir -p "$BB_DATA_DIR"'
  '';
  meta.mainProgram = "bbport";
}