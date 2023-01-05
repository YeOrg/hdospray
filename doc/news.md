News, Updates, and Announcements
================================

- Dec 2, 2022: Version v0.11.0

      - Texture Transforms
      - UDIM textures
      - Texture channel masks
      - Tonemapper settings
      - Interactive rendering reworked
      - Bug fixes

- Sep 20, 2022: Version v0.10.1

      - USD 22.08 support. 21.08 is still default in superbuild for houdini. 
      - Golden image tests added
      - Github actions workflow added for CI
      - Bug fixes

- June 10, 2022: Version v0.10

      - USD 21.08 support, which is now the default in superbuild.  20.08 still supported if enabled.
      - Instance, Element, and Primitive ID buffers added
      - Testing added with image comparisons
      - Added facevarying texture and color support.

- Feb 15, 2022: Version v0.9

      - Adding depth AOV
      - Adding normals AOV
      - Additional material parameters
      - Fixing generic texcoord naming
      - Update to OSPRay 2.9
      - Adding cylinder light
      - Adding camera depth of field
      - Minimal support for geomsubset materials
      - Ptex support currently deprecated in OSPRay.  Will be re-enabled upon OSPRay module_ptex release.

- August 24, 2021: Version v0.8

      - Update to USD 20.08.
      - Houdini support.
      - Color buffer AOV support.
      - Superbuild script added.
      - Added camera light visibility flags.
      - Add thin materials.


- May 28, 2021: Version v0.7
    
      - Update to OSPRay 2.6.0
      - Ptex support re-enabled.
      - Numerous light and material updates. Added disk lights.
      - Toggle for light enabled vs visible.
      - Animation update fixes.
      - Support for curves.


- October 12, 2020: Version v0.6
    
      - Update to OSPRay 2.4.0
      - Added support for opacity textures.
      - Added adaptive view port scaling. The viewport is
        now scaled to reach a given interactive framerate (e.g., 10fps).
      - Fixed some bigs with animated shapes (e.g., ARKit data sets).
      - Added support to toggle the visibility of light sources and shapes.


- July 31, 2020: Version v0.5

    - Update to OSPRay 2.2.0.
    - Added UsdLux light support.
    - Asynchronous rendering.
    - Dynamic resolution for greater interaction.
    - Pixel Filters.
    - Subdivision surfaces interpolation mode fixes.


- June 15, 2020: Version v0.4

    - Update to OSPRay 2.1.0.
    - Update to USD 20.05.


- September 15, 2019: Version v0.3

    - Subdivision surfaces support.
    - GUI options for usdview.
    - Various bug fixes.


- April 30, 2019: Version v0.2.2

    - Various bug fixes.
    - OSPRay version updated to 1.8.5.
    - CMake targets.
    - Added animation support.


- Mar 7, 2019: Version v0.2.1

    - Bug fixes.
    - Ptex updates.
    - Documentation.


- Feb 28, 2019: Version v0.2.0

    - Initial Beta release version 0.2.0 is now available on the [HdOSPRay
GitHub page](https://github.com/ospray/hdospray/releases/v0.2.0).
This replaces the previous repository and versioning which was on my
personal github account.


