# Playlist Downloader icon set

- `icons/hicolor/scalable/apps/<APP_ID>.svg` - main app icon (128px grid, GNOME style, no baked shadow)
- `icons/hicolor/symbolic/apps/<APP_ID>-symbolic.svg` - 16px monochrome icon (recolored by the desktop)
- `icons/hicolor/NxN/apps/<APP_ID>.png` - PNG fallbacks 16-512
- `snippets/` - metainfo branding colors, .desktop Icon= line, manifest install commands
- `preview.png` - light/dark, small sizes, footprint grid, Flathub banner preview

Run `./rename-app-id.sh your.real.AppId` first. Then check with:
`flatpak run --command=flatpak-builder-lint org.flatpak.Builder appstream your.metainfo.xml`
