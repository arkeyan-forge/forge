# Build the player kits from public sources

> **Purpose:** inventory the disc dependencies of kit production and remove them in checked steps for issue #72. · **Status:** current (steps 1 to 5 implemented; step 6 workflow implemented, hosted proof pending; step 7 designed, 2026-11-13) · **Tags:** [Xbox]

This page records the public header emitter and the remaining proposal. Step 1 changes no runtime, builder, kit
format or production workflow. The target is three kits, Windows x64, macOS arm64 and Linux x64, each containing
all six games. Public source metadata remains the authority for supported releases. No private input contents,
addresses, hashes, instruction words or disc payloads
are reproduced here. Private filenames below identify dependencies only; the study uses public code.

## Recommendation and scope

Generate the runtime's context and translation-policy headers from public sources. Replace the runtime's four
compile-time image/code bounds with an immutable layout supplied by one small object compiled during the player's
install. The player's generated PowerPC code keeps its existing constants and lookup formula. Validate both sides
against the same public ABI contract and the parsed executable before entering guest code.

Remove both fingerprint generation and copying from kit production. Retain the existing supported-release
allowlists and edition metadata report. Reference fingerprints cannot be reconstructed from an unknown edition
alone: optional local reference generation is a separate feature, with reduced address diagnostics until a valid
reference is available. This proposal does not automatically support games whose code differs from supported code.

The alternatives are to compile the affected runtime sources during install, which adds sources, headers and
compile recipes to the kit, or to make all translated code use a dynamic layout, which changes a much hotter path.
The small layout object keeps the runtime prebuilt and avoids changing translated calls. Simply deleting the
header hash checks or substituting dummy image constants would leave incompatible machine code in the kit.

## Inputs and their current consumers

In the table, “production” means running `tools/make_kit.sh`; “install” means Arkeyan-Builder on the player's machine.
The script's initial checks and edition block are top-level shell code, not named functions.

| Input and fields | Reader: file and function/block | When and why |
|---|---|---|
| `private/<game>/ppc/ppc_config.h`: `PPC_IMAGE_BASE`, `PPC_IMAGE_SIZE`, `PPC_CODE_BASE`, `PPC_CODE_SIZE` | `tools/make_kit.sh`: initial header checks, `build_runtime`, `assemble`; `tools/arkeyan-builder/kit_commands.py`: top-level packing | Production requires and copies the header into `stub-ppc`, compiles the runtime against it, and writes its digest to `kit.txt`. Packing copies the resulting per-game runtime objects; disc layout survives in those objects even though the header itself is not shipped. |
| Same header: `PPC_CONFIG_*` policy macros, inclusion guards and optional `PPC_INCLUDE_DETAIL` include | `thirdparty/XenonRecomp/XenonRecomp/recompiler.cpp`: `Recompiler::Recompile`; runtime translation units including `ppc_config.h` and `ppc_context.h` | Production compiles the runtime with the same register policy as the translated code. The policy comes from public TOML, whereas the four bounds come from the parsed disc image. The optional detail include does not require `ppc_detail.h` in the current stub build because that mode is not enabled. |
| `private/<game>/ppc/ppc_context.h`: generated prefix and public context template, including `PPCContext`, `PPCFunc`, `PPCFuncMapping`, `PPCFuncMappings`, lookup and call macros | `tools/make_kit.sh`: initial existence checks, cross-game `cmp`, `build_runtime`, `assemble`; runtime includes | Production currently obtains this through disc translation and hashes the generated header. It is a build dependency but contains a reproducible wrapper around public source, not image-specific values. Cross-game equality checks the template version, not all policy macros in the separate config. |
| `private/editions/<game>.txt`, copied to `config/<game>/fingerprints.txt`: `kind`, `name`, `address`, `words`, `hash`, `head`, `hi`, `lo`, ordered callee lengths/hashes | `tools/make_kit.sh`: edition block and `assemble`; `tools/arkeyan-builder/fingerprint.cpp`: `Read`, `Find`; `tools/arkeyan-builder/main.cpp`: `EditionReportCommand` | Optional production input, copied whenever present, even with no disc environment variables. Install-time diagnostics compare a different edition against supported-code references. The file contains reference addresses and structural metadata as well as hashes; it is not just a list of disc identifiers. |
| `SSA_ISO`, `GIANTS_ISO`, `SWAPFORCE_ISO`, `TRAPTEAM_ISO`, `SUPERCHARGERS_ISO`, `IMAGINATORS_ISO`: disc selected for each game; disc file `default.xex` and its loaded code sections | `tools/make_kit.sh`: edition block; `tools/arkeyan-builder/edition_prints.cpp`: `main`; `tools/arkeyan-builder/edition.cpp`: `ImageCode` | Production builds the host `ArkeyanEditionPrints` if any variable is set, then reads available discs to refresh references. Missing inputs can leave old reference files in place. The Linux container inherits the resulting files through the mounted checkout rather than needing the discs itself. |
| `private/libs/<game>/libSSARecompLib.a`: translated function-start symbol names | `tools/arkeyan-builder/edition_prints.cpp`: `main`; `tools/arkeyan-builder/fingerprint.cpp`: `FunctionStarts`, `Make` | Production uses the archive as a function-boundary index for fingerprints. Its object code is not linked into the kit by this step. The historical archive basename is used for every game. |
| `games/<game>/addresses.h`: `GAME_HOOK_*` symbols and image-address constants | `tools/arkeyan-builder/edition_prints.cpp`: `main`; `fingerprint.cpp`: `ReadAddresses`, `Make`; runtime/game/engine hook compilation | Public, already committed metadata used in production both for fingerprint labels and compiled hooks. It remains release-specific even after dynamic layout is introduced. No private address header is needed. |
| `games/<game>/game.txt`: `xex_sha256`, `image_sha256`, `title_id`, `releases`; matching release fields in `games/<game>/info.cpp` | `tools/make_kit.sh`: top-level consistency checks and `assemble`; `game_config.h`: `ReadGame`, `FindGame`, `FindGameByImage`; `main.cpp`: `IdentifyGame`; `edition.cpp`: `ReadDiscEdition`, `MatchEdition`; `runtime/host/launcher.cpp`: `CheckGameSource` | Public, disc-derived identification metadata. Production verifies the hash lists against `info.cpp` and packages `game.txt`; the runtime compiles its release table. Install computes the player's executable/image hashes and checks the allowlists before translation. These values do not require a disc on CI. |
| `games/<game>/config/*`: primary TOML, switch-table TOML and SSA's symbol text; executable paths, function/helper addresses, sizes, context functions, invalid instructions and mid-assembly hooks | `tools/make_kit.sh`: `assemble`; `tools/arkeyan-builder/main.cpp`: `TranslateCode`; `thirdparty/XenonRecomp/XenonRecomp/recompiler_config.cpp`: `RecompilerConfig::Load` | Production copies public configuration without opening a disc. Install supplies `default.xex`, and XenonRecomp applies the public translation knowledge to it. Layout independence does not make these addresses edition-independent. |

The copied `game.txt` also describes shader extraction (`shaders`) and the runtime shader pass (`movie_portal`,
`movie_press`, `movie_shaders`). These are public recipes, not production disc reads. `name`, `runtime`, `version`,
`toml` and `id` select naming/configuration rather than supply executable contents.

### Exact runtime dependencies on the generated layout

| File:function or definition | Current use | Required change |
|---|---|---|
| `runtime/loader.cpp`: `LoadXex` | Check image bounds, copy image, initialize the code lookup range and populate `PPCFuncMappings` | Compare the parsed image and code-section envelope with the install-generated expected layout, then initialize an immutable active layout before touching the table. Validate every mapping before writing it. |
| `runtime/loader.cpp`: `TableEntry`, `TableBegin`, `TableEnd`, `SetEntryReadOnly` | Compute and protect the function table inside the guest reservation | Use the active layout with the existing entry spacing and protection granules; preserve boundary rounding. |
| `runtime/loader.cpp`: `CreateHostThunk` | Search code padding, patch the lookup table temporarily, remember the next slot | Initialize the cursor from the validated layout on module load, not from a translation constant in a function-local static; retain serialization and protection restoration. |
| `runtime/indirect_call_guard.cpp`: `GuestCodeAddress`, `CallGuest` | Aligned code-range test and host-to-guest lookup | Use the validated active code range and the same lookup formula as generated code. Preserve the current refusal behavior. |
| `runtime/kernel/memory.cpp`: `__imp__MmQueryAddressProtect`, `__imp__NtQueryVirtualMemory`, `__imp__MmQueryStatistics` | Image membership, allocation base/size and image-page statistics | Read active image bounds and preserve the existing page/statistics semantics. |
| `thirdparty/XenonRecomp/XenonUtils/ppc_context.h`: `PPC_LOOKUP_FUNC` | Locate a host function pointer using image end and the target's offset from code base | Runtime compilation resolves the four bound names through the active layout; translated compilation retains its generated constants. Both must resolve to exactly the same table entries. |
| `ArkeyanRecompLib/indirect_call_guard.h`: `PPC_CALL_INDIRECT_FUNC` | Forced into translated sources; aligned code-range guard before lookup | Keep constant bounds for translated code. Its call target must agree with the runtime table, including gaps between code sections. |

`GuestMemory::Reserve` in `runtime/memory.cpp` already reserves a fixed guest address space independently of the
disc. Validate the image and table fit that reservation, have no arithmetic overflow, and do not overlap protected
or reserved ranges. Derive code bounds exactly as `Recompiler::Recompile` does: the envelope of all sections marked
as code, including intervening gaps. Taking only the first section would change lookup and thunk behavior.

The public policy inputs in `[main]` are `skip_lr`, `skip_msr`, `ctr_as_local`, `xer_as_local`, `reserved_as_local`,
`cr_as_local`, `non_argument_as_local`, `non_volatile_as_local`, with defaults from `RecompilerConfig::Load`.
Their emitted names are `PPC_CONFIG_SKIP_LR`, `PPC_CONFIG_SKIP_MSR`, `PPC_CONFIG_CTR_AS_LOCAL`,
`PPC_CONFIG_XER_AS_LOCAL`, `PPC_CONFIG_RESERVED_AS_LOCAL`, `PPC_CONFIG_CR_AS_LOCAL`,
`PPC_CONFIG_NON_ARGUMENT_AS_LOCAL`, `PPC_CONFIG_NON_VOLATILE_AS_LOCAL`.
Some affect the context layout; others affect register saving or which registers diagnostics may inspect.
`runtime/loader.cpp`: `UnmappedFunction`, `runtime/indirect_call_guard.cpp`: `SsaBadIndirectCall` and
`runtime/thread.cpp`: `PrintGuestLocation` have explicit policy branches. All runtime and adapter includes must use
the same policy as the player's translation, even where no layout constant is referenced directly.

### Inputs that remain exclusively at install

`tools/arkeyan-builder/main.cpp`: `ReadDisc` extracts `default.xex` and inventories the player's files and shaders.
For the loaded executable image and the shader archives selected by `shaders`, extraction in `ReadDisc` and later
translation in `TranslateShaders` remain local. `TranslateCode` produces `ppc_config.h`, `ppc_context.h`, mapping/detail headers and
translated sources; `Compile` compiles those sources and the shader cache; `Link` combines their objects with the
kit's runtime. The runtime shader pass also executes the locally built game during a normal install, so a complete
real install is not a permitted check for this study.

The kit stub deliberately supplies empty `ppc_func_mapping.cpp` and `shader_cache.cpp`, and `build_runtime` excludes
`libArkeyanRecompLib` and `libArkeyanShaderCache` from the built link inputs. No translated game source, image,
shader cache, archive, disc art or icon is needed for production. `runtime/CMakeLists.txt` guards local game-icon
inputs with `ARKEYAN_KIT_BUILD`; the Mac kit uses the public bundle icon/template. Install-time icon, menu-music,
art and texture extraction must stay local as well.

`private/toolchains/`, `private/ffmpeg/` and the optional scanner word-list filename are not disc-derived inputs.
Their locations do not justify caching or mounting all of `private/`. Dependencies must be fetched from their
pinned public releases; scanner rules for CI can use synthetic sentinel words without player logs.

## Changes by input, component and risk

### Public context and ABI generation

Extract a public-header emission path from the repository's existing `Recompiler::Recompile` implementation.
It must accept the public config and context template without parsing an executable. Reuse its policy parser and
wrapper writer so a second implementation cannot drift. No external code is required.

Production generates a runtime-facing config with policy flags and symbolic access to the active layout, plus the
same context wrapper the translator emits. The kit still ships the public template as `share/ppc_context.h`.
The generated context digest can remain `ppc_context_sha256`, because the generated wrapper is disc-independent.
Test its byte identity against the translator's wrapper using an invented image. Do not hash the raw template and
pretend it is the existing generated-header digest.

Replace `ppc_config_sha256.<game>` in the new format with a public ABI/policy identifier per game. The identifier
covers normalized policy flags/defaults, context template/wrapper revision, pointer/context layout assumptions and
the layout/lookup contract version. Verify context size, alignment and the relevant register offsets with compile
probes on each target. Reject unexpected or incompatible generated policy flags before compiling. The full generated
config checksum may still be a local cache key; it no longer belongs in release metadata.

Runtime and builder risk: identical context text does not prove identical preprocessing, register policy or target
ABI. Keep an explicit target/version contract and compare the generated policy to the public expected policy;
never allow a missing legacy hash to silently accept a new-format kit.

### Step 1: done

`tools/abi/headers.py` emits `ppc_context.h` and a policy-only `ppc_config.h` using public inputs. The shared writer
is `RecompilerHeaders::ConfigPrefix` / `RecompilerHeaders::Context`, maintained in `patches/xenonrecomp.patch`.
`Recompiler::Recompile` calls those same functions; its output bytes and translation behavior are preserved.
The emitter calls `RecompilerConfig::Load`, sharing all policy parsing and defaults, but never calls
`Recompiler::LoadConfig`, loads an executable, or follows its executable/patch/output paths. It parses the public
TOML and its public switch-table TOML only. There is no separate policy-default table.

From the repository root, with a host Clang compiler and initialized submodules:

```sh
python3 -I tools/abi/headers.py emit ssa .scratch/abi/public-ssa
python3 -I tools/abi/headers.py check
python3 -I tools/abi/headers.py test
```

The script builds a small host emitter from repository sources, applying the canonical dependency patch if needed.
An already patched checkout must use the current patch; reset stale dependency patches using the normal checkout
procedure before configuring. The `emit` command takes any of the six game IDs and an output folder. The shared
policy prefix retains the translator's macro order, spelling, guards and newlines. The emitted config closes that
guard without image/code bounds or the optional detail include. The generated context includes this config and
the same public template with exactly the translator's wrapper. No private input is needed for emission.

The equivalent CMake tools are available with `ARKEYAN_TOOLS_ONLY=ON` or `SKY_TESTS_ONLY=ON`:

```sh
cmake --build build-tools --target ArkeyanPublicHeaders ArkeyanPublicHeadersFixture
build-tools/tools/abi/ArkeyanPublicHeaders games/ssa/config/SSA.toml \
    thirdparty/XenonRecomp/XenonUtils/ppc_context.h .scratch/abi/public-ssa
python3 -I tools/abi/headers.py test --emitter build-tools/tools/abi/ArkeyanPublicHeaders \
    --fixture build-tools/tools/abi/ArkeyanPublicHeadersFixture
```

`check` reads every existing `private/<game>/ppc` directory, compares context bytes and complete policy definition
lines, and compiles/runs two host layout probes. They compare `sizeof`, alignment and every context field's
`offsetof`, with conditional LR/MSR members and function-type assertions. An absent directory yields `SKIP`; a
present directory with missing headers fails. Results contain only `PASS`, `FAIL` or `SKIP` and the game ID; private
header contents, compiler diagnostics and layout measurements are never reported or copied. The check is included
in `tools/test_all.sh --quick`, using its already built emitter.

`test` exercises the full translation header path with an invented section and no guest instructions for all six
public policy profiles, empty/omitted defaults and every LR/MSR combination. It compares the same bytes, macros and
compiled layouts without private files, and verifies rejection of missing/changed headers and extra/changed policy
definitions. CTest registers this as `ArkeyanPublicHeadersTest` on native Clang builds. Local validation passed all
six private comparisons and host probes on macOS arm64. Invented-fixture outputs also matched the writer before
extraction byte for byte. Windows x64 and Linux x64 layout validation remain target checks for later CI.

Step 2 must supply the four bound names (`PPC_IMAGE_BASE`, `PPC_IMAGE_SIZE`, `PPC_CODE_BASE`, `PPC_CODE_SIZE`) to
runtime consumers through a validated layout contract. This policy-only config deliberately supplies no substitute
bounds and cannot yet replace the kit's translated config. Keep translated code's constants, lookup formula and
public policy unchanged; validate the full code-section envelope, table bounds and initialization order as described
below. Kit header hashes, builder validation and production input selection still use their existing paths; the new
ABI/policy identifier and format transition remain subsequent work.

### Local layout object and runtime initialization

After translation, Arkeyan-Builder validates the generated four bounds against the executable it already parsed.
It generates a small translation unit in its local work tree, using the generated headers to initialize an exported
expected-layout record. The record includes the public ABI version and identity of the executable used for
translation, so another allowed release with a coincidentally equal layout cannot be substituted. This object is
created only at install, never packed or cached by CI.

`Compile` builds that source with a dedicated recipe or a proven compatible existing recipe; `ObjectList` and `Link`
must include it exactly once. `tools/arkeyan-builder/kit_commands.py` needs to express any new recipe and public
header include. Preserve the local object in runtime replacement-library link commands; it is not a kit input.
`tools/arkeyan-builder/kit_relink.py` packs Arkeyan-Builder's own objects, so it needs regression verification rather
than a copy of the player's layout object. Normal CMake builds must also produce the record from their own generated headers; otherwise only
kit-built games would work. Plan these changes together, including ordinary and cross builds.

`LoadXex` reads the expected record, validates executable identity and all four parsed bounds, and initializes the
runtime layout before imports, thunks or guest threads. The record and active layout are read-only thereafter.
Keep the old table location and pointer stride, the unmapped-function behavior and page protection policy. Check
zero/empty code, alignment, overflow, table bounds, entry point and mapping ranges before any guest-memory write.
The public `PPCContext` definition must remain identical across runtime and translated translation units.

The primary risks are initialization order, a mismatched lookup formula, different code-section envelopes, an
unvalidated mapping and indirect-call overhead. Test numerical equivalence of constant and dynamic lookup over
invented layouts, including code gaps and protection boundaries. Generated calls retain their constants, limiting
performance changes to runtime-side consumers. Measure host-side lookup loops in unit tests before selecting an
accessor that adds an out-of-line call to every runtime lookup.

### Kit metadata, caching and version transition

Add an explicit format/layout-contract field and per-game ABI/policy identifiers to `kit.txt` in a future kit
version. Retain `platform`, `version`, `build`, `games`, `game_version.<game>` and compiler version fields, all asset
names, command placeholders and launcher events. The existing layout digest fields remain mandatory only for
legacy kits; the new builder dispatches on format, checks all required new fields and rejects unsupported versions.
Legacy builders reject a new kit through their missing legacy hash checks; the updated launcher should reject an
unsupported format before invoking a builder. Coordinate the launcher contract/version update in its own repo.

`TranslateCode` currently stamps all game config files except `game.txt`, the translator, the shared context and the
two expected header hashes. Change its stamp to include the selected public ABI contract. Keep diagnostic-only
fingerprints outside translation inputs. Give local layout generation its own key including executable identity,
generated config, ABI contract and generator version. Preserve recompilation and relink keys in `Compile` and
`Link`; content changes must invalidate the right work, while a runtime-only update should reuse translated code.
Retain the existing work-folder/resume rules, including `ssabuild-work` compatibility. Cross builds currently reuse
native translation; they must validate that reused translation and regenerate/compile the target's local object.

Risk: stale stamps can bypass new validation or combine old translation with new objects. Validate required
artifacts even on resume, test interrupted upgrades, and keep the previous installed executable until linking and
signing succeed. A format transition may rebuild once; subsequent runtime-only updates must remain incremental.

### Discs, archive symbols and public release metadata

Delete the production edition block and the unconditional optional fingerprint copy, plus the initial private-header
existence/equality checks and private-header digest reads. `build_runtime` takes only public-generated headers.
The six disc environment variables and `private/libs/<game>/libSSARecompLib.a` cease to be production dependencies.
Do not merely unset the variables: an old editions directory must not affect the output of a clean kit build.

Keep public `game.txt`, `info.cpp`, `addresses.h`, config TOML, switch tables and SSA symbols. Keep the production
consistency checks and both `IdentifyGame` and `CheckGameSource` allowlists. Install continues computing executable
and loaded-image identity from the player's disc. Title/media identifiers alone do not prove code compatibility.
The risk of broadening acceptance is avoided by keeping changed-code editions refused, even though runtime layout
is now dynamic. Supporting another code edition still needs reviewed hooks and translation configuration.

## Can fingerprints be generated during install?

Yes, for a **known supported reference disc**, after successful identification and translation. The existing
`fingerprint::Make` can consume the player's parsed image, the translated function starts and the matching public
address labels. Prefer the generated mapping as the boundary source rather than requiring a separately built
archive. A future local generator would need those public labels in the kit and an explicit reference-cache
interface; neither is an existing builder feature. Cache references only in the player's work/data tree, keyed by
game, supported image identity, public address/config revision and fingerprint algorithm version.

No, for an arbitrary **unknown-code disc** without a known reference. A fingerprint generated from that disc can
describe its functions but cannot associate them with the supported release's hook/data names. Searching a disc
against fingerprints just made from itself does not solve relocation. Hashes alone do not supply that missing
semantic association, and the FNV fingerprints are matching heuristics, not integrity proofs.

Recommended first release: ship no `fingerprints.txt`. `EditionReportCommand` already tolerates its absence;
`ReadDiscEdition`, `MatchEdition` and `EditionReport` still report executable/image identity, region, version and
languages. Known executable and same-image editions keep the same acceptance rules. Unknown-code reports lose the
address-match section and unknown-code installs stay refused. State that diagnostic reduction in release notes;
it is the one intentional capability change, not an unchanged-report claim.

If equivalent address diagnostics are required later, add explicit local reference creation from a supported disc,
then allow the player to select that local reference when reporting another disc. Refuse wrong-game, stale or
malformed references and retain ambiguous/not-found results. Never upload that cache or put its values in a kit.
Without a supported reference available to the player, equivalent cross-edition diagnostics remain unavailable.

### Step 5: done

Edition reports now say "Address matching is unavailable for this report." when a known game's unknown-code disc
has no address diagnostics. Missing, empty and comment-only reference files are optional; executable/image identity,
region, version and languages remain in the report. Known executables and same-image editions retain their existing
support decisions and report text, while unknown code remains refused even when optional references find functions.
Reports with usable references retain their address matches and not-found entries. Install refusal text is unchanged.

`ArkeyanEditionTest` covers these cases with invented metadata and reference words, including exact report comparisons.
Build it in the tests-only configuration and run with all disc environment variables unset; no game or launcher is
needed. The report's maintained documentation is [Edition finder](../../tools/edition_finder.md). Local reference
generation remains a follow-up, outside this step.

Validation: the requested tests-only configure and `ArkeyanEditionTest` build passed; its disc-free run passed
99 checks with no failures. The report-import self-test passed all 8 checks, and the three edited documentation
pages passed the link checker.

## Step 6: candidate workflow (implemented; hosted run pending)

First hosted runs (2026-11-13): the macOS runner needed `molten-vk` (MoltenVKShaderConverter, for FSR's Metal shaders);
the Windows kit cross-built on Linux met `#include <Windows.h>` (plume, XenonUtils) on a case-sensitive file system:
`tools/make_kit.sh` puts `cmake/toolchains/mingw-case/` (headers forwarding to mingw's lower-case names) on `CPATH` on
a Linux host only. A patch to plume instead broke the re-application of `plume.patch`, whose hunk shares those lines.

`.github/workflows/kits.yml` builds the three six-game kits using step 4's `KIT_LAYOUT_RECORD=1` path. It adds no
builder or `make_kit.sh` changes, and leaves the Mac build recipe unchanged. The workflow is separate from the
existing public host tests in `.github/workflows/ci.yml`.

Triggers:
- `workflow_dispatch` with no `launcher_tag`: build, check and upload three ZIP artifacts only. No launcher
  repository variable or release credential is needed.
- `workflow_dispatch` with `launcher_tag=vX.Y.Z`: the same builds, then `attach` after all three jobs pass. Set the
  repository variable `KITS_LAUNCHER_REPO=owner/repository` and secret `KITS_RELEASE_TOKEN` with write access to
  that repository's releases. The specified launcher release must already be a draft with launcher assets and
  `SHA256SUMS` attached.
- Push to `ci/kits`: build and check the three kits and retain their ZIP artifacts; `attach` is skipped. No tag,
  pull-request or other branch trigger publishes kits.

| Job / output | Runner and toolchain | Production command and prerequisites |
|---|---|---|
| Windows x64 kit | `ubuntu-24.04`, Linux-hosted llvm-mingw 20260922 | `tools/ci/fetch_llvm_mingw.sh` checks the pinned archive before extraction and supplies `LLVM_MINGW_ROOT`; build minimal Windows FFmpeg with `tools/ffmpeg/build.sh`, then `tools/make_kit.sh windows`. Four compile jobs, 240-minute timeout. |
| macOS arm64 kit | `macos-15`, Xcode 16.4, deployment target 13.0 | Assert arm64 and selected Xcode/SDK/Metal tools; fetch the existing Mac-hosted llvm-mingw license input; build host FFmpeg, then `tools/make_kit.sh macos`. Two compile jobs, 240-minute timeout. |
| Linux x64 kit | `ubuntu-24.04`, Docker amd64 with `tools/linux/kit.Dockerfile` | Explicitly run `tools/make_kit.sh linux` inside the image: step 4 emits public headers with zig and builds FFmpeg through `tools/ffmpeg/build.sh`. Keep the image's pinned compiler, glibc target and C++ library. Four compile jobs, 360-minute timeout. |
| Attach candidates | `ubuntu-24.04`, only a tagged manual run after all kits pass | `tools/ci/kits.py publish` checks exactly the three canonical ZIPs, consistent game versions and an existing draft; merges checksums and updates the hidden games line, then downloads and verifies the uploaded kits and sums. 30-minute timeout. |

`KIT_DEPS=/tmp/kit-deps` is identical on every host and inside Docker; `FFMPEG_WORK` is its `ffmpeg/work` subfolder.
Windows FFmpeg and Mac host FFmpeg receive explicit prefixes under that root. The Linux job mounts the dependency
root at the same absolute path; FFmpeg is built inside the image. This fixes the remaining path-sensitive Linux
relink object noted in step 4. No public build dependency is placed under `private/`. Both llvm-mingw host archives
are pinned in `tools/ci/fetch_llvm_mingw.sh`; the Linux archive's SHA-256 was verified against GitHub's release-asset
digest. FFmpeg's source pin and minimal static LGPL recipe remain in `tools/ffmpeg/build.sh`.

Every kit job runs `tools/ci/check_kit.sh`: scan the completed directory with `tools/kit_scan.sh`, validate the ZIP
inventory and every checksum with `tools/ci/kits.py inventory`, re-extract into a fresh temporary folder, verify
`files.sha256` independently, then scan the extracted kit. Mac program signatures and relative rpaths are checked;
Mac and Linux executable modes are checked without starting a kit program. The inventory requires format 2,
`layout_record=1`, `abi.context`, all six `abi.policy.<id>` and game-version fields, and rejects legacy header
metadata, unsafe archive entries, fingerprints and install-only layout record objects. It allows the required
LGPL `relink/` materials. Only the three final ZIPs are uploaded, with seven-day artifact retention; no dependency
or build caches are introduced in this pass.

Actions are pinned to revisions. Build jobs have read-only repository permission and no release credential.
Attachment is serialized across tags. It preserves launcher checksum entries, replaces the hidden games/version
line, refuses a published release or a different existing kit, and keeps the draft unpublished. A matching existing
kit can be retried. The helper rechecks the draft immediately before writes and verifies uploaded bytes afterward;
the owner must leave the draft unchanged during attachment. The launcher's manual `tools/release_kit.sh` remains
an alternative in its own repository.

### First hosted run and remaining evidence

Start with a manual run without a tag, or a push to `ci/kits`. It must show three successful six-game public builds,
clean scans of both directories and extracted archives, matching manifests, valid format-2 ABI fields and exactly
the three named ZIP artifacts. `attach` must be skipped without destination settings or a release token. Record
elapsed time and peak disk use before tuning parallelism or adding caches; Linux previously took 100–166 minutes
in Docker on a Mac, so the workflow allows six hours for its first hosted run.

Then a tagged manual run against a prepared launcher draft must show the three canonical kit assets, preserved
launcher checksum entries, one matching games/version line, successful download verification and an unpublished
draft. No hosted workflow or real release attachment has been run locally. Local checks cover the helper tests,
YAML parsing, shell syntax, `tools/test_all_test.py` and the documentation checker.

Same-commit reference-kit comparison with `tools/parity/public.rules`, native fixture links against extracted kits,
cold/warm cache measurements and the real-computer parity rows remain separate proof requirements. This workflow
does not make the record kit the default and does not establish game-install or play parity.

## Parity gate (the owner's requirement, 2026-11-12)

The goal is the same behaviour with no disc in the kit build, and the least disturbance for players. Every switch
below stays off by default (old path) until its row passes; the old path is deleted only after a release ships with
the new one. Each row is a script under `tools/` that prints pass/fail per game and target, never a private value.

| What | Compared | Pass when |
|---|---|---|
| Kit contents | the 0.9.6 kit and the new kit of the same commit, per target | the same file list and bytes, except the listed intended differences (`fingerprints.txt` gone, the new `kit.txt` fields, the layout-record sources and recipe) |
| Generated headers | step 1's emitted context and policy against `private/<game>/ppc/` | byte-identical, all six games |
| Install | an install with the old kit and one with the new kit, same disc, per game and target | identical translated sources, shader cache, game files and normalized compile/link commands; the only new link input is the layout record |
| The game | the same deterministic test runs (title, one level, the capture runs) with each install | identical logs (timestamps removed) and frame hashes; `tools/test_all.sh` (full) green on both |
| Editions | the edition report of every disc we hold, old against new | identical, except the address-match section, which says it isn't available |
| Updating | a 0.9.6 install updated by the new launcher and kit | resumes or relinks as a runtime-only update, saves and settings untouched, no new translation |
| Refusals | a wrong or old kit, a wrong layout record, an unknown edition | refused with a clear message, the previous install kept |
| Real computers | the owner's Mac, Windows and Linux PCs, then the testers | every game installs and plays as with 0.9.6 |

The comparison tool for every row: `tools/tree_parity.py <old> <new> --rules <rules>` (folders or zips; names and
counts only, never contents), with `tools/parity/kit.rules` for kits. A kit build is reproducible: the same commit's
macOS kit built twice gave 1446 identical files (2026-11-12), so any difference it reports comes from the change.

## Ordered, independently testable PRs

| Order and scope | Proof required before the next PR |
|---|---|
| 1. Public ABI/header emission only; keep existing production path | An invented executable fixture and each public policy profile yield identical policy macros and generated context wrapper through translation and disc-free emission. Compile context layout/offset probes on all targets. No supported-release list or runtime behavior changes. |
| 2. Layout contract and runtime consumers, initially opt-in | Add a synthetic host layout test covering multiple code sections/gaps, lookup arithmetic, table protection, invalid mappings, overflow, image mismatch and thunk exhaustion. Constant and dynamic paths select identical entries and memory-query results. Keep ordinary CMake builds working with a locally generated record. Existing `ArkeyanThunkSlotTest` and `ArkeyanGuestThreadTest` cover adjacent behavior but do not replace the new layout tests. |
| 3. Builder local-object generation, ABI validation and format dispatch | Extend `ArkeyanBuilderGuardTest`/`ArkeyanBuilderTest` with fake process results and invented metadata: legacy/new formats, wrong context/policy, missing fields, wrong image, local object linked exactly once, cross build, cancellation and failed relink preserving the previous install. Check cold install, no-op resume, config change and runtime-only update invalidation. |
| 4. Switch production to public headers; remove all disc/fingerprint reads | Build all six games for each target in a fresh public checkout with no private headers, editions, translated archive or ISO variables. Trace input access in a synthetic test and prove even stale private filenames cannot affect the recipe. Only public dependency download/build directories may be created. Inspect permitted kit inventory, commands, license/relink materials and run `kit_scan`. |
| 5. Edition diagnostics without distributed references | Update report text/docs to explain absent address matches, preserve known-XEX/same-image acceptance and unknown-code refusal. Extend synthetic `ArkeyanEditionTest` cases for missing references and metadata-only reports. Keep optional local reference generation as a follow-up PR only if address-report parity is required. |
| 6. Candidate workflow, caches and native fixture links | Run a cold and warm candidate build for all targets; unpack and verify all manifests, ABI policies, file lists, executable modes, commands and signatures. Link and run host unit fixtures on the target OS, inspect LGPL relink inputs, measure time/storage, and prove no player-derived artifact enters an upload or cache. |
| 7. Versioned contract and launcher release integration | Update the kit contract and launcher format expectations, publish to a draft using the existing helper, verify exactly the three canonical kit assets and checksum entries, consistent game versions and old/new-format rejection. Release only after the permitted compatibility evidence below is reviewed. |

## Step 2: done (2026-11-12)

What changed: the CMake option `ARKEYAN_LAYOUT_RECORD` (default OFF) builds the record-driven runtime; the code and its
checks are described in [loader_xex.md](../../runtime/xbox360/loader_xex.md#the-code-layout-record-arkeyan_layout_record-opt-in-72).
Two choices differ from the plan above:
- **The consumers' sources are untouched.** For runtime sources, `runtime/layout/ppc_config.h` comes before `ppc/`,
  includes the generated header for its policy macros and redefines the four bound names to read the active layout, so
  `PPC_LOOKUP_FUNC` and every consumer listed in "Exact runtime dependencies" compute through the same formula, with
  the same `unsigned long long` type. `CreateHostThunk`'s cursor stays a function-local static, initialized at the first
  call, which only happens after `LoadXex` has activated the layout. With the option OFF, `loader.cpp` preprocesses to
  the old text (`-E -P` identical) and compiles to the same instructions [V].
- **Identity is optional in the record.** A normal CMake build does not know which executable produced `ppc/`: its record
  (`runtime/guest_layout_record.cpp`) leaves the image hash empty and only the version and the four bounds are checked.
  A record with a hash also refuses another image with an equal layout (unit test).

Proof [V], on the Mac, every game built ON (`-DARKEYAN_LAYOUT_RECORD=ON`) with the prebuilt translated libraries:
- `ArkeyanGuestLayoutTest` (54 checks), `ArkeyanGuestThreadTest` built ON, `ArkeyanThunkSlotTest`: pass. Lookup loop
  over an invented 2 MB code range: 1.6-1.8 ns per lookup with the bounds compiled in, 1.9-2.0 ns from the record
  (a dependent loop that only looks up; the hot path is translated code, which keeps its constants).
- SSA (`level_027`), Giants (`level_203_the_machine`), Swap Force (the tutorial) and Trap Team (`level_313_tutorial`):
  one short run OFF and one ON each, isolated, the same presses. Same files opened (96, 28, 81, 46), no fault, no
  `[guard]` line, the level reached both ways, `[loader] code layout from the build's record` in every ON log. The runs
  are not deterministic (thread timing on a loaded machine), so frames match only where the scene is still (identical
  Giants frame at 30 s; others 13-30 dB at the same second, the same scene shifted in time).
- SuperChargers and Imaginators: `tools/regress/superchargers_lobby.sh` and `imaginators_hub.sh` on the ON builds
  (new game and continue to the hub): PASS, 16 and 13 checks (no fault, no new stub or guard, fps floor, saves, frames,
  sound).

### What step 3 (the builder writes the record at install) must produce

1. One source in the install's work tree defining `const GuestLayoutRecord g_guestLayoutRecord` (declared in
   `runtime/guest_layout.h`, which the kit must ship as a public header): `version = GuestLayoutVersion` (1), the four
   bounds of the generated `ppc_config.h` after checking them against the executable the builder parsed (code bounds =
   the envelope of the code sections), and `imageSha256` = the lowercase hex SHA-256 of that image, never empty in a
   builder record. It must include `guest_layout.h` only, or the generated `ppc_config.h` with
   `ARKEYAN_LAYOUT_CONSTANTS` defined, never `runtime/layout/ppc_config.h` without it.
2. The kit's runtime objects compiled with `ARKEYAN_LAYOUT_RECORD` and `runtime/layout` first on the include path,
   `guest_layout.cpp` among them, and **without** `guest_layout_record.cpp`'s object: `make_kit.sh` `build_runtime` and
   `kit_commands.py` must drop it; `Compile`/`ObjectList`/`Link` (and the relink path) add the local record object
   exactly once. A missing or duplicate record must fail the link, not start the game.
3. Cost to decide: with a hash in the record, `LoadXex` hashes the image at every start with the runtime's own
   `Sha256Hex` [?] (not measured; `CheckGameSource` already hashes the executable, and the image when the executable is
   unknown, so the hash could be passed along instead of computed twice).
4. Tests for step 3: the builder's record for an invented executable is accepted by `CheckGuestLayout`; a record from
   another image (equal layout, other hash) is refused; ON builds of the six games still boot (as above).

## Step 3: done (2026-11-12)

What changed, all opt-in: `KIT_LAYOUT_RECORD=1 tools/make_kit.sh` builds the kit's runtimes with `ARKEYAN_LAYOUT_RECORD`
and without `guest_layout_record.cpp`'s object, ships `share/guest_layout.h` and writes `layout_record=1` to `kit.txt`.
Without it the kit is built as before. Arkeyan-Builder sees `layout_record=` and, at the end of the `recompile` step (no new `@step`: the launcher's
checklist shows every step id as a row), checks the generated four bounds against `default.xex` as it parses it (code bounds: the envelope of the code
sections). It writes `layout/guest_layout_record.cpp` (version, bounds, image SHA-256, never empty) next to a copy of the
header, compiles it with `compile.args` (its own `obj/guest_layout_record.key`) and links it once. `Link` refuses a link
that does not name exactly one record (one in a layout-record kit, none in another), and the linker itself refuses a
missing or a second record. Commands, file names and freshness: [arkeyan-builder.md](../../tools/arkeyan-builder.md),
[kit_contract.md](kit_contract.md). Choices:
- **The header is in `share/`, not `include/`.** `include/`'s content is part of every game object's key: a default kit
  and a record kit therefore give the same keys, and moving between them recompiles only the record.
- **Hash cost: kept in `LoadXex`.** The runtime's `Sha256Hex` takes about 5 ms per MB on this Mac (7 to 25 MB images: 35
  to 125 ms per start, measured on invented buffers) [V]. Passing `CheckGameSource`'s hash along would save that but
  ties the launcher check to the loader, and that check hashes the image only for an unknown executable. Revisit if
  start time matters.

Proof [V], on the Mac (isolated, `../gamelock.sh`):
- `ArkeyanBuilderGuardTest` (92 checks) with invented data: a record with wrong, missing or out-of-range bounds or no
  hash is refused; the written record, compiled and linked by the real compiler with the runtime's `guest_layout.cpp`,
  is accepted for its image and refused for another hash with the same layout; no record or two records fail the link;
  compile and link expectations, a cancelled compile (no object, no key), a failed or cancelled link (the previous game
  kept, `link.key` removed), an old kit (no record compiled or linked; a kit record object refused).
- Kits (macOS, the six games): the default kit against one built from `next/0.10.0`: 1434 of 1447 files identical. The
  differences are Arkeyan-Builder and its relink objects (this change, including the new `guest_layout.cpp` object),
  the revision (`kit.txt` `build=`, the six `host_log.cpp.o`), `files.sha256` and `bin/XenosRecomp`. XenosRecomp's
  build-time rpath names the checkout, and the base snapshot's longer path shifts `__text` by 8 bytes (same symbols,
  same load commands): a kit depends on the length of the checkout's path, which step 6 must keep fixed. Default
  against record kit, same commit: `tree_parity.py ... --rules tools/parity/kit.rules` passes (1422 identical, 25
  allowed) without its step-4 `fingerprints.txt` line. Only `loader`, `kernel/memory` and `indirect_call_guard` differ
  among the runtime objects. The record kit's `relink/arkeyan-builder.py --link` rebuilds a working Arkeyan-Builder.
- Installs, SSA and Giants, from one disc each, with each kit at the same paths: `tree_parity.py ...
  --rules tools/parity/install.rules` passes (SSA 526 identical, Giants 4844). Every translated source, object and
  object key, the disc's files and the shader inputs are identical. The rules list each allowed difference: the record,
  its object, the game, the link, the Metal shaders. `tools/shaders/compare_cache.py`: the same 1122 and 4444 shaders,
  1 and 4 AIR sizes apart (the Metal compiler's own run-to-run noise). The link differs by exactly two inputs:
  `guest_layout.cpp.o` and the record.
- Updating: a default-kit SSA install updated with the record kit took 4 s: nothing translated, 1 of 125 files compiled
  (the record), one link. A rerun took 2 s, every step up to date. Going back to the default kit: nothing compiled,
  one link.
- Boots, same presses: SSA reaches `level_027` (`mission.str` at 20.6 s default, 22.1 s record), Giants
  `level_203_the_machine` (29.2 s both). No fault, no `[guard]`; the record builds log `[loader] code layout from the
  build's record`. A game linked with a record of the same layout but another hash stops before guest code: "default.xex is not the executable this build was
  recompiled from: another executable (image hash)", exit 1.

### What step 4 needs

1. Compile the record kit's runtimes against public headers. Only the three consumers changed when the bounds moved to
   the record, so the other objects do not read them, and step 1's emitted config should need no bounds [?]. Proof:
   the record kit's `lib/` built from public and from `private/` headers must match (`kit.rules`).
2. Replace `ppc_config_sha256.<id>`/`ppc_context_sha256` (still written and checked by `TranslateCode`, in both kits)
   with the ABI ids, and make the record flavour the default only after the parity rows pass on every target.
3. The Windows (lld, COFF) and Linux (zig) record kits are not built yet [?]: run the same kit and install parity there,
   and the guard test's compiler case there (it is skipped in a cross build).
4. Fix the checkout path for production builds (XenosRecomp above), or strip the rpath at link time.

## Step 4: done (2026-11-12)

What changed, all opt-in (`KIT_LAYOUT_RECORD=1`; the default kit, its checks and its fingerprints are as before):
- **The record kit is built from public sources.** `make_kit.sh` emits each game's `ppc_config.h` (policy only) and
  `ppc_context.h` with `tools/abi/headers.py emit` into `build-kit-abi/<id>/` (in the Linux image with zig's compiler)
  and compiles the runtimes against them. It reads no `private/<id>/ppc`, no `private/editions` (the edition block is
  skipped: no `*_ISO` variable is read, no `fingerprints.txt` shipped) and no `private/libs`. The public build
  dependencies (llvm-mingw, the FFmpeg prefixes and source) come from `KIT_DEPS` (default `private/`, as before) or
  their own variables; the Linux container mounts `KIT_DEPS` and `KIT_SCAN_WORDS` when they are outside the checkout.
  `KIT_HEADERS_FROM_PRIVATE=1` builds the same kit against `private/<id>/ppc`, for the parity gate only.
- **`kit.txt`, format 2**: `kit_format=2`, `layout_record=1`, `abi.context` (SHA-256 of the public `ppc_context.h`,
  byte-identical to the translator's) and `abi.policy.<id>` (SHA-256 of the `#define PPC_CONFIG_*` lines, in order, as
  `headers.py` compares them; `headers.py ids <folder>` prints both). No `ppc_config_sha256.*`/`ppc_context_sha256`.
- **Arkeyan-Builder** (`CheckKitFormat`, before any step) refuses another `kit_format`, a format 2 without
  `layout_record` or without a valid ABI id for each of `games=`, and a `layout_record` without `kit_format` (the step-3
  test kits). After XenonRecomp, a format-2 install checks the generated headers against the ABI ids ("this disc does
  not match the kit (ppc_config.h differs)"); the bounds stay the record's check. The translation stamp names the
  translated headers' own SHA-256s once they carry the kit's ABI: the very values a default kit checks. Only the
  stamp's `config=` hash changes between the two kits (it covers `config/<id>/fingerprints.txt`, which a record kit
  lacks): XenonRecomp runs once (1 to 2 s), writes the same files, and nothing is compiled again. A launcher refusing an unknown `kit_format` is a
  change in its own repository (step 7): today's launcher ignores the key, and the builder refuses the kit.
- **XenosRecomp (macOS) is linked with no build rpath**, `-Wl,-rpath,@executable_path` only: the checkout path's length
  no longer moves its code, so a kit no longer depends on where the checkout is (step 3's note).

Proof [V], on the Mac, at commit `07b70414` (the code of this step; later commits are docs and rules):
- **Public checkout.** A clone of this branch with no `private/`, submodules from the integration clone, `KIT_DEPS`
  inside its `.scratch/`: `KIT_LAYOUT_RECORD=1 tools/make_kit.sh macos windows linux` built the three kits, `kit_scan`
  clean. Against the record kits built here from `private/` headers, `tree_parity.py ... --rules
  tools/parity/public.rules`: macOS 1448 of 1448 files identical, Windows 1455 of 1455, Linux 1486 of 1488 (100
  and 166 min in Docker on this Mac). On Linux, Arkeyan-Builder's relink object of the FFmpeg decoder keeps debug
  info naming FFmpeg's include folder under `KIT_DEPS` (`./private/ffmpeg` against `./.scratch/deps/ffmpeg`), and
  `files.sha256` lists it: both listed in the rules. Every runtime object and library is identical on the three
  targets. The checkouts' paths differ in length: the kits do not depend on it.
- **Default against public record kit** (macOS, `kit.rules`): PASS, 1416 identical, 25 allowed (the six
  `fingerprints.txt` gone, `kit.txt` keys, `guest_layout.h`, `guest_layout.cpp.o` and the three consumers per game,
  `link.args`, `files.sha256`). Arkeyan-Builder and XenosRecomp are identical.
- **Installs, the six games** (each disc with the default kit and with the public kit, same paths):
  `install.rules` passes; the only difference beyond step 3's is `ppc/stamp.txt` (its `config=` hash covers
  `config/<id>/fingerprints.txt`, which the record kit lacks), now listed. Files identical / allowed (the shader caches' AIR, the record, the game, the link) /
  same shaders (`compare_cache.py`): SSA 525 / 1122 / 1122 shaders, 1 AIR size apart; Giants 4843 / 4446 / 4444, 3
  apart; Swap Force 2388 / 925 / 1081; Trap Team 1379 / 883 / 880; SuperChargers 2846 / 1048 / 1214; Imaginators 2925
  / 1045 / 1190 (AIR sizes: the Metal compiler's own noise). Every translated source and object is identical.
- **Boots**, the same presses, each install: no fault, no `[guard]`, the record line in every public-kit log. SSA
  reaches `level_027` both ways (194 lines naming it each), Giants `level_203_the_machine`, Swap Force the tutorial
  (the same 77 distinct files opened), Trap Team `level_313_tutorial`; SuperChargers and Imaginators pass
  `superchargers_lobby.sh` and `imaginators_hub.sh` (new game and continue to the hub), both kits.
- **Updating** (SSA): a default-kit install updated with the public kit took 5 s: XenonRecomp ran again (126 files,
  0 changed), 1 of 125 files compiled (the record), one link. A rerun: 1 s, every step up to date. Back to the default
  kit: 4 s, nothing compiled, one link.
- `ArkeyanBuilderGuardTest`: 113 checks (format dispatch, ABI ids, a default kit's stamp kept by a format-2 kit,
  refused policy and context, other bounds left to the record step).

Not proven yet [?]: the Windows and Linux kits installed and played (no Wine or Linux install run here); the
guard test's compiler case on Windows and Linux; the launcher's own refusal of an unknown format. Step 6 (CI) needs:
the three `make_kit.sh` runs with `KIT_LAYOUT_RECORD=1` and `KIT_DEPS` from pinned public downloads (llvm-mingw's
Linux-hosted archive for Windows; FFmpeg built by `tools/ffmpeg/build.sh` with `FFMPEG_WORK`), `tools/parity/public.rules`
against a reference kit of the same commit, `kit_scan`, and one fixed `KIT_DEPS` path (the Linux relink object
above); the checkout's own path no longer matters (macOS and Windows proven; Linux always builds at `/src`).
Making the record kit the default is a decision after the real-computer rows of the parity gate.

“Unchanged” means the supported discs, translated output semantics, table layout, installed names/resources,
launcher events, resume rules and runtime behavior remain equivalent. Kit bytes and manifest fields intentionally
change; ZIP/signature/compiler metadata also preclude treating binary equality as the only proof. Compare the
permitted file inventory and normalized link/compile recipes; unaffected public libraries should remain identical
under a pinned toolchain, while layout consumers gain the documented descriptor dependency.

Public CI can prove absence of private production inputs and synthetic ABI/install behavior. It cannot prove that
each real disc still builds and plays correctly. A later, separately permitted private acceptance run must compare
old/new installs for every supported game/platform: identification, generated-header/translation equivalence,
shader set, link inputs, installed layout, incremental updates and the existing isolation/game checks. Retain only
sanitized pass/fail summaries in public evidence, never private values or payloads. Those checks include a runtime
shader pass and games; none are run for this study. If permission remains unchanged, real-install parity remains an
explicit unverified release gate rather than a claimed result.

For documentation changes, run `python3 -I tools/check_docs.py` and explicitly check this page and its folder index,
since the default checker discovers tracked files. The step 1 commands above run only host tools and unit probes;
no game, launcher, kit build or real install is needed.
