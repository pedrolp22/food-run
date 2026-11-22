# Third-party notices

Food Run's original code is GPL-3.0-or-later. The components and models below
retain their own licenses. Copies of library notices are in `licenses/` in a
binary release and `external/licenses/` in the source repository.

## Libraries

| Component | Copyright / author | License | Source |
| --- | --- | --- | --- |
| SDL 2.30.12 | Sam Lantinga and SDL contributors | zlib | https://github.com/libsdl-org/SDL/tree/release-2.30.12 |
| GLAD 0.1.36 | David Herberth | MIT; Khronos specifications under Apache-2.0 | https://github.com/Dav1dde/glad/tree/v0.1.36 |
| Khronos platform headers | The Khronos Group Inc. | MIT-style permission notice | `external/glad/khrplatform.h` |
| stb_image | Sean Barrett and contributors | MIT or public domain | https://github.com/nothings/stb |

Compiler runtime libraries retain their upstream licenses and runtime exceptions.
Portable Windows builds use static MSVC runtime libraries or MinGW runtime
libraries; Linux builds include libgcc and libstdc++ with the GCC Runtime Library
Exception. SDL and the runtime libraries are linked without changing their source.
GCC runtime exception and MinGW-w64 notices are included with the library notices.
MSVC runtime code is redistributed as part of the statically linked application
under the Visual Studio runtime redistribution terms.

## Models

The original attribution text is also included beside each model in
`assets/models/<model>/license.txt`, in both source and binary releases.
Food Run adapts model scale, materials, lighting and placement at runtime and
includes generated material maps for the bench, trash can and tree.

- This work is based on "LowPoly Road Barrier - Low Poly Free !"
  (https://sketchfab.com/3d-models/lowpoly-road-barrier-low-poly-free-876717cbfdd841c6987cbc53b3f52643)
  by yAncioso (https://sketchfab.com/yAncioso), licensed under CC-BY-NC-4.0
  (https://creativecommons.org/licenses/by-nc/4.0/).
- This work is based on "street bench low poly"
  (https://sketchfab.com/3d-models/street-bench-low-poly-38a2be20eca64bd98b6b88483e80eab6)
  by Vedrow (https://sketchfab.com/Vedrow), licensed under CC-BY-4.0
  (https://creativecommons.org/licenses/by/4.0/).
- This work is based on "Low Poly Psx Street lamp"
  (https://sketchfab.com/3d-models/low-poly-psx-street-lamp-81a8d1861bbd4cef8ab1aed2986bf873)
  by SaukrDev (https://sketchfab.com/SaukrDev), licensed under CC-BY-4.0
  (https://creativecommons.org/licenses/by/4.0/).
- This work is based on "Traffic Cone (Low Poly)"
  (https://sketchfab.com/3d-models/traffic-cone-low-poly-89a750d568cc4817b163daec27625dd0)
  by samcurtiswright (https://sketchfab.com/samcurtiswright), licensed under CC-BY-4.0
  (https://creativecommons.org/licenses/by/4.0/).
- This work is based on "Trash [Low Poly]"
  (https://sketchfab.com/3d-models/trash-low-poly-aab5c5cf92b44aae91e1ea084f95ea5c)
  by Islide (https://sketchfab.com/Islide), licensed under CC-BY-4.0
  (https://creativecommons.org/licenses/by/4.0/).
- This work is based on "Leaf Tree - PS1 Low Poly"
  (https://sketchfab.com/3d-models/leaf-tree-ps1-low-poly-d799c08100974e1ba352fcd646cb0694)
  by Wersaus33 (https://sketchfab.com/wersaus33), licensed under CC-BY-4.0
  (https://creativecommons.org/licenses/by/4.0/).

The road barrier's noncommercial license applies to that model. The bundled asset
collection therefore has restrictions beyond the GPL license on our code. Replace
that model or obtain permission from its author before commercial distribution.
