#version 400

uniform bool flip = false; ///< Flip vertically.

out INTERFACE {
	vec2 uv; ///< UV coordinates.
} Out ;

/**
  Generate one triangle covering the whole screen,
   with according positions and UVs based on vertices ID.
 \verbatim
 2: (-1,3),(0,2)
	 *
	 | \
	 |	 \
	 |	   \
	 |		 \
	 |		   \
	 *-----------*  1: (3,-1), (2,0)
 0: (-1,-1), (0,0)
 \endverbatim
*/
void main(){
	vec2 temp = 2.0 * vec2(gl_VertexID == 1, gl_VertexID == 2);
	Out.uv = flip ? vec2(temp.x, 1.0-temp.y) : temp;
	gl_Position.xy = 2.0 * temp - 1.0;
	gl_Position.zw = vec2(1.0);
}
