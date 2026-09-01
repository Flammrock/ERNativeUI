# Scaleform SDK reference model

Status: external reference model; Elden Ring bindings still under investigation

This document describes the public Scaleform concepts that guide the binary
analysis. It is not an ABI declaration for Elden Ring. The game may wrap,
specialize, inline, or omit any SDK interface, and no SDK signature becomes a
safe hook merely because the executable contains a similarly named RTTI type.

The tested executable does contain RTTI for types such as
`Scaleform::GFx::Loader`, `Scaleform::GFx::MovieDef`,
`Scaleform::GFx::Value`, `CS::CSScaleformLoader`,
`CS::CSScaleformMovieDef`, `CS::CSScaleformSwfPlayer`, and
`CS::CSScaleformValue`. That makes the public model a useful search guide, not
proof of Elden Ring's object layouts or calling contracts.

## Public lifecycle model

Autodesk's archived Scaleform documentation describes the normal lifecycle as:

```text
GFx::Loader::CreateMovie(path)
        |
        v
GFx::MovieDef                         shared loaded movie data
        |
        | CreateInstance(...)
        v
GFx::Movie                            one playback/ActionScript state
        |
        +-- Advance(delta)            timeline, queued input, ActionScript
        +-- HandleEvent(event)        keyboard/controller-style input
        +-- GetVariable / Invoke      path-based C++ -> ActionScript bridge
        +-- Capture / display handle  render snapshot
        `-- Release                   movie-instance lifetime
```

`MovieDef::CreateInstance` returns a referenced movie instance. Complex
`GFx::Value` objects belong to that movie's ActionScript runtime and must be
released before the movie dies. `Advance` performs more than animation: it
also processes queued input and ActionScript work. In a multithreaded renderer,
movie mutation and render submission are separated by capture snapshots.

For ERNativeUI this means that loading a GFX file is only one part of a custom
interface. A production integration also needs the game's movie registry,
advance/input route, render registration, resource bindings, and ordered
teardown. Calling a plausible loader function alone would leak or leave a
movie that is neither interactive nor rendered.

## Public value and invocation model

Scaleform exposes two related C++ bridges:

- `GFx::Movie` has path-based operations such as `GetVariable`, `SetVariable`,
  and `Invoke`.
- `GFx::Value` is a direct reference to a primitive, object, array, or display
  object. Complex values support operations such as `GetMember`, `SetMember`,
  `Invoke`, `SetDisplayInfo`, and `SetText`.

The public API distinguishes creating an ActionScript object from creating a
display object on the stage. `Movie::CreateObject` can create an ActionScript
object or class instance, but the documented `GFx::Value` API does not itself
provide a general operation that places a new display object on the stage.
Runtime visual creation still depends on the movie's ActionScript/timeline or
another game-specific binding.

This distinction is directly relevant to ERNativeUI:

- resolving `WindowList/.../Text_0` and changing its text is value access;
- cloning a GFX placement offline changes the movie definition;
- creating a focusable native row requires Elden Ring's row/control model;
- creating an arbitrary new visual tree at runtime requires a proven stage
  construction path, not only `CreateObject`.

## Public event directions

The public model has two directions:

```text
game/native code -> Scaleform
    Movie::HandleEvent
    Movie::Get/SetVariable
    Movie::Invoke
    Value direct-access operations

Scaleform/ActionScript -> game/native code
    FSCommandHandler
    ExternalInterface
    FunctionHandler-backed function objects
```

The static `02_040_optionsetting.gfx` inspection found no menu-specific
ActionScript listener, `ExternalInterface`, or `FSCommand` use. Its settings
behavior is therefore expected to be driven chiefly by Elden Ring's native
`MenuWindow`/scene/control layer. Other movies may use different mechanisms.

The executable's RTTI and loader setup prove that Elden Ring installs a
`CSScaleformFsCommandHandler`. Follow-up analysis also proved that its handler
override is a no-op (`ret 0`) in the tested build. It is therefore not a
discovered event bridge for Game Options or an ERNativeUI custom movie; see
[Scaleform movie loading and lifecycle](SCALEFORM_MOVIE_LIFECYCLE.md).

## Working correspondences to test

These are hypotheses for targeted Ghidra queries. They are deliberately not
production names yet.

| Elden Ring evidence | Public Scaleform analogue | Current status |
|---|---|---|
| Path resolver at RVA `0x74B140` | Variadic path formatting followed by component-wise GetMember-equivalent traversal from a source `CSScaleformValue` | Confirmed; it is not a proved `Movie::GetVariable` wrapper |
| UTF-16 text setter at RVA `0x74AE50` | `GFx::Value::SetText(wchar_t const*)` | Strongly inferred from type checks, call shape, and live behavior |
| Wrapper cluster around `0x74AE50` | Specialized TextField text, color, and scroll helpers | Individual operations mapped; two narrower property names remain inferred |
| `CSScaleformSwfPlayer` | owner/wrapper of a `GFx::Movie` playback instance | Confirmed; raw movie is at `+0x18` |
| `CSScaleformMovieDef` | game wrapper around shared `GFx::MovieDef` data | Confirmed; raw definition is at `+0x10` |
| `GfxRepositoryImp` | game resource/path registry above `GFx::Loader` | Hypothesis |
| `CSScaleformStep` | startup/resident-resource state machine | Confirmed; it is not the per-frame player loop |
| `SceneObjProxy` and control classes | native binding between menu model and named display objects | Hypothesis |

Each row must be confirmed by callers, vtables, object offsets, and lifetime
behavior. Similar class names alone are insufficient.

## Binary-analysis questions derived from the model

1. What do the still-unmapped loader flag bits at `CSScaleformLoader +0x18`
   mean, and which resource-owner states may safely reach the confirmed
   `0xD72CC0 -> 0x112CF30` boundary? (`0xD72CC0` itself supplies zero
   per-call flags.)
2. Which operation in the confirmed create/register path at `0xD7C900`
   completes render registration for a new `CSScaleformSwfPlayer`?
3. Which thread executes the confirmed player loop
   `0xD7ADE0 -> 0xD73850`, whose raw-movie virtual `+0xC0` strongly matches
   `Movie::Advance`?
4. Where does capture/render submission occur relative to that advance call?
5. Where are keyboard, mouse, and controller actions translated into
   `GFx::Event` objects or higher native menu actions?
6. Which wrapper functions around `CSScaleformValue` implement member access,
   invocation, display properties, text retrieval, and destruction?
7. Does `SceneObjProxy` retain a complex `GFx::Value`, a path, a player
   reference, or some combination of them?
8. How are movie unload and `GFx::Value` cleanup ordered relative to native
   `MenuWindow`, row, and callback destruction?

## Primary references

- [Autodesk: Scaleform `GFx::Loader`](https://help.autodesk.com/cloudhelp/ENU/Scaleform-Help/cpp_ref/00889.html)
- [Autodesk: `MovieDef::CreateInstance`](https://help.autodesk.com/cloudhelp/ENU/Scaleform-Help/cpp_ref/01126.html)
- [Autodesk: Scaleform `GFx::Movie`](https://help.autodesk.com/cloudhelp/ENU/Scaleform-Help/cpp_ref/00974.html)
- [Autodesk: `Movie::Advance`](https://help.autodesk.com/cloudhelp/ENU/Scaleform-Help/cpp_ref/00975.html)
- [Autodesk: C++ to ActionScript communication](https://help.autodesk.com/cloudhelp/ENU/Scaleform-Help/scaleform_help/game_communication/c_actionscript.html)
- [Autodesk: Direct Access API](https://help.autodesk.com/cloudhelp/ENU/Scaleform-Help/scaleform_help/game_communication/direct_access.html)
- [Autodesk: Scaleform `GFx::Value`](https://help.autodesk.com/cloudhelp/ENU/Scaleform-Help/cpp_ref/01708.html)
- [Autodesk: processing input events](https://help.autodesk.com/cloudhelp/ENU/Scaleform-Help/scaleform_help/integration_tutorial/integration_game_engine/integration_processing.html)
- [Autodesk: multithreaded rendering concepts](https://help.autodesk.com/cloudhelp/ENU/Scaleform-Help/scaleform_help/renderer_guide/multi_threaded_concepts.html)
