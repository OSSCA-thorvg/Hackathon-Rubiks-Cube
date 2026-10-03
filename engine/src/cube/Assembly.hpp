#pragma once

#include <optional>
#include <vector>

#include "cube/Cube.hpp"
#include "cube/Face.hpp"

/**
 * Reading a painting of stickers as a cube, or saying why it is not one.
 *
 * Somebody holding a muddled cube can colour it onto the unfolded net, and
 * what arrives here is 6N^2 colours in the order `surface_stickers()` counts
 * them. Most such arrays are not cubes: no turning reaches them, and a solver
 * handed one would search for a position that does not exist.
 *
 * So this is a gate, and the shape of the gate is the whole contract.
 * `assembled()` hands back a cube or nothing at all, which is what keeps
 * `Cube`'s promise intact -- every cube in the domain is one that turning
 * could have reached, and this proves it a second way rather than weakening
 * it. `read_painting()` answers the other question a person actually has,
 * which is *what* is wrong and *where*.
 *
 * Colours and blamed indices retain the legacy surface order at this input
 * boundary. Validated cubes store only their six native outward-facing grids.
 */
namespace rubiks::cube {

/** What is wrong with a painting, in the order the checks run. */
enum class PaintFault {
    None,

    /** A colour is not on exactly N^2 stickers. */
    ColourCount,

    /**
     * A colour is on the wrong number of stickers *within one orbit*.
     *
     * A piece never leaves the set of slots it can reach, and from five by
     * five up the centres are several such sets rather than one -- so a white
     * centre and a red centre swapped between two of them leaves every total
     * where it was and still cannot be turned to.
     */
    OrbitCount,

    /**
     * The colours are laid out for a cube with different faces opposite.
     *
     * Which colours never share a piece is a fact about a cube one can read
     * off a painting, and a cube built to another scheme -- blue opposite
     * white rather than green -- reads differently. It is not a mistake; it is
     * a cube this build does not solve, and saying so beats calling it
     * impossible.
     */
    OppositePairs,

    /**
     * A piece wears colours no piece of a real cube wears, or wears them the
     * wrong way round.
     *
     * The second case is the one worth naming: three colours that do belong to
     * a corner still cannot be painted in mirror order, because no turn ever
     * reverses a corner.
     */
    ImpossiblePiece,

    /** Two slots hold the same piece. */
    RepeatedPiece,

    /** The corners cannot all be turned upright, however they are twisted. */
    CornerTwist,

    /** An odd number of edges of one depth are turned round. */
    EdgeFlip,

    /**
     * The corners sit in an order the edges cannot answer for.
     *
     * A three by three only. On bigger cubes an inner slice swaps a pair of
     * wings while moving nothing but centres nobody can tell apart, so the
     * count is not observable and this is not a rule -- it is the four by
     * four's familiar parity, and applying the three by three's rule to a big
     * cube would refuse cubes that really do solve.
     */
    Permutation,
};

/** What a painting is, and if it is nothing, why. */
struct PaintReading {
    PaintFault fault = PaintFault::None;

    /**
     * The stickers to point at, numbered as `surface_stickers()` counts them.
     *
     * The order is the contract between this and the drawing that highlights
     * them; it is not an implementation detail either may choose for itself.
     * Empty when nothing can usefully be pointed at.
     */
    std::vector<int> blamed;

    /**
     * Which of the twenty-four ways up the reading is of.
     *
     * A painting is read in every orientation, because how somebody held their
     * cube is their business. This is the one that answered, or -- when none
     * did -- the one that had the least to complain about.
     */
    int orientation = 0;
};

/** Every sticker of a cube, in the order `surface_stickers()` counts them. */
[[nodiscard]] std::vector<FaceColor> painting_of(CubeView cube);


/** What is wrong with a painting, or that nothing is. */
[[nodiscard]] PaintReading read_painting(int size,
                                         const std::vector<FaceColor>& painting);

}  // namespace rubiks::cube
