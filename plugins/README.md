# CCCaster Plugins Directory

This folder is scanned by the CCCaster plugin host to discover plugin manifests. Each
plugin should reside in its own subdirectory containing a `plugin.toml` manifest and
the compiled plugin library.

Example structure:

```
plugins/
  replay-takeover/
    plugin.toml
    replay_takeover.dll
```

