# Andesite Editor (WIP)
Small lightweight and extremely fast shader editor using nodes!

The graph automatically compiles nodes into the source code of a shader written in the Slang Shading Language.
All inputs are values that are uploaded via push constants, not baked into the shader, so you can take the shader straight from the editor into your own game/app!

Sets out to accomplish some goals of mine:
- Make it easy to iterate on a working shader
- Allow direct integration into other applications ie feed into another renderer
- Provide flexible export options, ie from Slang -> GLSL/HLSL/Slang (duh)


Acknowledgements
-  [ImNodeFlow](https://github.com/Fattorino/ImNodeFlow/tree/master) - It's basically required for my app to function, and I only got inspiration to make this when seeing it on the ImGui extensions page.
