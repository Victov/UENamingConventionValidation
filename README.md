# UE5 Naming Convention Validation
Original readme contents below.
## This fork

This fork adds/modifies the following:

- Refactored code to follow the [Epic C++ Coding Standard](https://docs.unrealengine.com/5.1/en-US/epic-cplusplus-coding-standard-for-unreal-engine/) which mostly means the following:
    -- Change widespread use of `snake_case` to `CapitalCase`
    -- Remove usage of `auto` in favour of explicit types.
    -- Rename certain booleans to have their proper `b` prefix.
    -- Change usage of size-less `int` to sized `int32`
- (WORK IN PROGRESS) Fix deprecation warnings and bring code up to date with UE5.1.
- (WORK IN PROGRESS) Add sensible defaults to the validator based on [Epics Naming Standard](https://docs.unrealengine.com/5.1/en-US/recommended-asset-naming-conventions-in-unreal-engine-projects/) so that the plugin works out of the box without configuration for 90% of use cases.

## Original Readme

This plug-in allows you to make sure all assets of your project are correcly named, based on your own rules. 

You can validate individual asset names by defining a prefix and / or a suffix based on the asset type.

But you can also implement more complicated validation rules, like making sure all assets inside a folder have a keyword in their name.

Check the [documentation](https://theemidee.github.io/UENamingConventionValidation/) for all the informations.
