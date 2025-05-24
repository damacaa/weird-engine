#version 430 core

in vec2 TexCoords; // Received from vertex shader, can be used for texture mapping or conditional logic

out vec4 FragColor;

// TBO: Assumed to be bound to a texture unit (e.g., unit 0)
// The TBO is expected to contain vec4 data (e.g., GL_RGBA32F format)
uniform samplerBuffer tboDataSampler;

// SSBO: Defines a structure for items and a buffer block
// Assumed to be bound to SSBO binding point 0
struct MyDataItem {
    vec4 itemColor;     // Example: a base color (RGBA)
    float itemIntensity; // Example: an intensity value
    // Add other members as needed, ensuring std430 layout compatibility (padding etc.)
    // For simplicity, keeping it basic. Note that vec4 is 16 bytes, float is 4 bytes.
    // In an array of structs, if itemIntensity was not last, padding might be inserted by the GLSL compiler
    // to ensure the next struct starts at a base alignment of vec4 (16 bytes).
    // Here, size of MyDataItem is 16 (vec4) + 4 (float) = 20.
    // If used in an array, next MyDataItem will start at offset 32 (multiple of 16).
    // This means 12 bytes of padding after itemIntensity.
    // To avoid this, one might add dummy floats or ensure struct size is a multiple of vec4 alignment.
    // Or, use arrays of basic types directly if complex structs are not strictly needed.
    // For this example, we'll assume the driver handles padding or we only access items[0].
};

layout(std430, binding = 0) buffer MySSBOBlock {
    MyDataItem dataItems[]; // Array of our custom data items
};

void main()
{
    // Fetch data from the TBO
    // texelFetch takes samplerBuffer and an integer index.
    // We'll fetch the first element (index 0) from the TBO.
    // vec4 tboColorContribution = texelFetch(tboDataSampler, 0);

    // Fetch data from the SSBO
    // Access the first element (index 0) from the SSBO.
    // Ensure that the buffer bound to 'binding = 0' actually contains at least one MyDataItem.
    MyDataItem ssboStructData = dataItems[int(0.1 * gl_FragCoord.x) % 16];

    // Combine the data to determine the final fragment color.
    // Make the contributions distinct to easily verify functionality.

    // TBO contributes to Red and Green channels.
    FragColor.r = ssboStructData.itemColor.r;
    FragColor.g = ssboStructData.itemColor.g;

    // SSBO contributes to Blue and Alpha channels.
    // Use the intensity from the SSBO to modulate its color contribution.
    FragColor.b = ssboStructData.itemColor.b * ssboStructData.itemIntensity;
    FragColor.a = 1.0f; // Use alpha from SSBO's color

    // Example of using TexCoords if needed:
    // if (TexCoords.x < 0.5) {
    //     FragColor.r = 1.0; // Make left half of quad red
    // }

    // For this demo, we are primarily focused on buffer data.
}
