1. Bump version in `meson.build`
2. Rename `spec/sessiond-$VERSION.spec`
    - Bump Version
    - Adjust source URL
    - Add changelog entry
3. Ensure `spec/sessiond.rpkg.spec` reflects packaging changes
4. Test spec with `tb rpkg-install`
5. Ensure `spec/sessiond-$VERSION.spec` reflects rpkg spec
6. Commit with `Update version to $VERSION`
7. Tag release
8. Push commits and tag
