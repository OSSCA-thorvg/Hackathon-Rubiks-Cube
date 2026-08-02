#pragma once

namespace rubiks::app {

/**
 * Initializes the ThorVG runtime used by the Rubik cube engine.
 *
 * @return true when the runtime is initialized or was already initialized.
 */
[[nodiscard]] bool initialize() noexcept;

/**
 * Shuts down the ThorVG runtime when it is currently initialized.
 */
void shutdown() noexcept;

/**
 * Reports whether this application owns an active ThorVG initialization.
 *
 * @return true when initialize() has completed successfully.
 */
[[nodiscard]] bool is_initialized() noexcept;

}  // namespace rubiks::app
