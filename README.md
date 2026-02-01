# ZMK Module: Settings RPC with Custom Web UI

This repository contains a ZMK module for managing keyboard settings via a web interface using an **unofficial** custom Studio RPC protocol. It provides remote control of activity settings (sleep/idle timeouts) for both standalone and split keyboards.

Basic usage is similar to the official template. Read through the
[ZMK Module Creation](https://zmk.dev/docs/development/module-creation) page for
details on how to configure ZMK modules.

## Features

- **Activity Settings Management**: Control sleep and idle timeouts via web interface
- **Split Keyboard Support**: Synchronized settings across central and peripheral halves
- **Custom Studio RPC Protocol**: Protobuf-based communication for settings management
- **React Web UI**: Modern web interface for device configuration
- **Real-time Notifications**: Receive settings updates from all connected devices

### Core Implementation

This module includes the following components:

- Core settings protocol: `proto/zmk/settings/core.proto` and `core.options`
- Settings RPC handler: `src/studio/settings_handler.c`
- Event relay system: `src/events/` (for split keyboard synchronization)
- Configuration flags in `Kconfig`
- Test suite: `./tests/studio`

### Extending with Custom Protocols

The module also includes a template for adding your own custom RPC protocols:

- Custom protocol template: `proto/zmk/template/custom.proto` and `custom.options`
- Custom handler: `src/studio/custom_handler.c`

### Web UI for Custom Protocols

The `./web` directory contains a React-based web interface based on the
[vite template `react-ts`](https://github.com/vitejs/vite/tree/main/packages/create-vite/template-react-ts)
(generated via `npm create vite@latest web -- --template react-ts`) and the React hook library
[@cormoran/react-zmk-studio](https://github.com/cormoran/react-zmk-studio).

For more details, refer to the
[react-zmk-studio README](https://github.com/cormoran/react-zmk-studio/blob/main/README.md).

## Setup

You can use this ZMK module with the following setup:

1. Add this module as a dependency to your `config/west.yml`.

   ```yaml
   # config/west.yml
   manifest:
     remotes:
       ...
       - name: cormoran
         url-base: https://github.com/cormoran
     projects:
       ...
       - name: zmk-module-settings-rpc
         remote: cormoran
         revision: main # or specific commit hash
       ...
       # The below settings are required to use the unofficial Studio custom RPC feature
       - name: zmk
         remote: cormoran
         revision: v0.3-branch+custom-studio-protocol+activity
         import:
           file: app/west.yml
   ```

2. Enable the feature flags in your `config/<shield>.conf`:

   ```conf
   # Enable ZMK Studio
   CONFIG_ZMK_STUDIO=y

   # Enable settings RPC module
   CONFIG_ZMK_SETTINGS_RPC=y
   CONFIG_ZMK_SETTINGS_RPC_STUDIO=y
   ```

3. (Optional) Configure activity settings in your `<keyboard>.keymap` if you want default values:

   ```dts
   / {
       behaviors {
           // Your keymap behaviors
       };
   };
   ```

## Development Guide

### Setup

There are two west workspace layout options.

#### Option 1: Download Dependencies in Parent Directory

This is West's standard approach. Choose this option if you want to reuse dependent projects in other Zephyr module development.

```bash
mkdir west-workspace
cd west-workspace # This directory becomes the West workspace root (topdir)
git clone <this repository>
# rm -r .west # If it exists, remove to reset workspace
west init -l . --mf tests/west-test.yml
west update --narrow
west zephyr-export
```

The directory structure becomes like below:

```
west-workspace
  - .west/config
  - build : build output directory
  - <this repository>
  # other dependencies
  - zmk
  - zephyr
  - ...
  # You can develop other zephyr modules in this workspace
  - your-other-repo
```

You can switch between modules by removing `west-workspace/.west` and re-executing `west init ...`.

#### Option 2: Download Dependencies in ./dependencies (Enabled in Dev Container)

Choose this option if you want to download dependencies under this directory (similar to `node_modules` in npm). This option is useful for specifying cache targets in CI. The layout is easier to manage if you want to isolate dependencies.

```bash
git clone <this repository>
cd <cloned directory>
west init -l west --mf west-test-standalone.yml
# If you use the dev container, start from the commands below.
# The above commands are executed automatically.
west update --narrow
west zephyr-export
```

The directory structure becomes like below:

```
<this repository>
  - .west/config
  - build : build output directory
  - dependencies
    - zmk
    - zephyr
    - ...
```

### Dev Container

The dev container is configured for setup option 2. The container creates the following volumes to reuse resources among containers:

- `zmk-dependencies`: Dependencies directory for setup option 2
- `zmk-build`: Build output directory
- `zmk-root-user`: /root, same as ZMK's official dev container

### Web UI

Please refer [./web/README.md](./web/README.md).

## Testing

**ZMK Firmware Tests**

The `./tests` directory contains test configurations for POSIX to verify module functionality and configurations for the Xiao board to confirm builds work correctly.

Tests can be executed with the following command:

```bash
# Run all test cases and verify results
python -m unittest
```

If you want to execute West commands manually, use the following (note: for zmk-build, results are not verified):

```bash
# Build test firmware for Xiao
# `-m tests/zmk-config .` means tests/zmk-config and this repo are added as additional Zephyr modules
west zmk-build tests/zmk-config/config -m tests/zmk-config .

# Run ZMK test cases
# -m . is required to add this module to the build
west zmk-test tests -m .
```

**Web UI Tests**

The `./web` directory includes Jest tests. See [./web/README.md](./web/README.md#testing) for more details.

```bash
cd web
npm test
```

## Publishing Web UI

### GitHub Pages (Production)

GitHub Actions are pre-configured to publish the web UI to GitHub Pages.

1. Visit Settings > Pages
2. Set source as "GitHub Actions"
3. Visit Actions > "Test and Build Web UI"
4. Click "Run workflow"

The Web UI will be available at
`https://<your GitHub account>.github.io/<repository name>/`, for example: https://cormoran.github.io/zmk-module-settings-rpc.

### Cloudflare Workers (Pull Request Preview)

For previewing web UI changes in pull requests:

1. Create a Cloudflare Workers project and configure the following secrets:

   - `CLOUDFLARE_API_TOKEN`: API token with Cloudflare Pages edit permission
   - `CLOUDFLARE_ACCOUNT_ID`: Your Cloudflare account ID
   - (Optional) `CLOUDFLARE_PROJECT_NAME`: Project name (defaults to `zmk-module-web-ui`)
   - Enable the "Preview URLs" feature in the Cloudflare project

2. Optionally, set up an `approval-required` environment in GitHub repository settings requiring approval from repository owners

3. Create a pull request with web UI changes - the preview deployment will trigger automatically and wait for approval

## Sync Changes from Template

By running `Actions > Sync Changes in Template > Run workflow`, a pull request is created to your repository to reflect changes from the template repository.

If the template contains changes in `.github/workflows/*`, you must register your GitHub personal access token as `GH_TOKEN` in the repository secrets.
The fine-grained token requires write permissions for contents, pull requests, and workflows.
For more details, see [actions-template-sync](https://github.com/AndreasAugustin/actions-template-sync).

## More Information

For more information on modules, you can read through the
[Zephyr modules page](https://docs.zephyrproject.org/3.5.0/develop/modules.html),
[ZMK's page on using modules](https://zmk.dev/docs/features/modules), and
[Zephyr's West manifest page](https://docs.zephyrproject.org/3.5.0/develop/west/manifest.html#west-manifests).
