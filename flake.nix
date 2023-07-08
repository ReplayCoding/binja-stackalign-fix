{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";
  };
  outputs = {
    self,
    nixpkgs,
  }: let
    forSystems = nixpkgs.lib.genAttrs nixpkgs.lib.systems.flakeExposed;
  in {
    devShells = forSystems (
      system: let
        pkgs = nixpkgs.legacyPackages.${system};
      in {
        default = pkgs.gccStdenv.mkDerivation {
          name = "devshell";
          hardeningDisable = [ "all" ];
          buildInputs = with pkgs; [dbus];
          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config

            gdb
          ];
        };
      }
    );
  };
}

