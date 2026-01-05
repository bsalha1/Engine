#pragma once

namespace Engine
{
    /**
     * @brief Interface for drawable objects.
     */
    class Drawable
    {
    public:
        virtual ~Drawable() = default;

        virtual void draw() const = 0;
    };
}