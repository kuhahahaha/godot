# Superseded build diagnostic

The first current-source editor build used `target=editor dev_build=yes` without `tests=yes`. Its binary SHA was `7156f3c09bfd99fc35014d92ba44b20f81b92d903330f809626022bb96a9211f`; all `--test` selectors returned rc1 with:

```text
`--test` was specified on the command line, but this Godot binary was compiled without support for unit tests.
```

This diagnostic is superseded by the tests-enabled build in this package: build rc0, editor SHA `fb8693bd5c159a724793b17f23581b1fdc2e1ba9875c0195834883ec79cc736a`, and all recorded selectors rc0.
