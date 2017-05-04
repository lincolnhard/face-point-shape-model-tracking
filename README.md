# face-point-shape-model-tracking
Modified and inspired from https://github.com/MasteringOpenCV/code/tree/master/Chapter6_NonRigidFaceTracking

The method is similar to ASM, but it use template matching for landmark tracking instead of active contour.

I use 24 landmarks to build shape model, which are label 27-36, 38-44, 48, 49, 51, 53, 54, 56, 58 in MUCT annotation.
http://www.milbo.org/muct/muct-landmarks.html

There are 8 bases in shape model, 4 rigids and 4 non-rigids.

The 4 rigid bases are representing scale, in-plane rotation, translation in x direction, and translation in y direction.

The 4 non-rigid bases are head pitch, certain facial expression, head yaw, certian facial expression:<p>
![4](https://cloud.githubusercontent.com/assets/16308037/25690810/b80c56ea-30c7-11e7-9537-55f3d2bba67d.gif) ![5](https://cloud.githubusercontent.com/assets/16308037/25690809/b80c10c2-30c7-11e7-82b4-e0c2f09798a2.gif) ![6](https://cloud.githubusercontent.com/assets/16308037/25690811/b81a9ab6-30c7-11e7-9d8d-2327c3913276.gif) ![7](https://cloud.githubusercontent.com/assets/16308037/25690808/b7cd03dc-30c7-11e7-8803-73f7a1d08705.gif)

By projecting landmarks to the subspace formed by 4 rigid and 4 non-rigid bases, then project back, we can have a
standard enough face shape. Furthoemore, limit the weight of each non-rigid basis using its standard deviation makes
every face shape common.

Tracking code is re-written in C-style, and scaled down patch matching search area for faster performance. 

![bug](https://cloud.githubusercontent.com/assets/16308037/25141237/8727ccbe-2495-11e7-80a8-8394ccba63a8.jpg)
