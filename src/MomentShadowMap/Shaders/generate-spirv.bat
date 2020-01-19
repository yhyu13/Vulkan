glslangvalidator -V GBuffer.vert -o GBuffer.vert.spv
glslangvalidator -V GBuffer.frag -o GBuffer.frag.spv
glslangvalidator -V Deferred.vert -o Deferred.vert.spv
glslangvalidator -V Deferred.frag -o Deferred.frag.spv
glslangvalidator -V Shadow.vert -o Shadow.vert.spv
glslangvalidator -V Shadow.frag -o Shadow.frag.spv
glslangvalidator -V MomentShadowMapH.comp -o MomentShadowMapH.comp.spv
glslangvalidator -V MomentShadowMapV.comp -o MomentShadowMapV.comp.spv
pause