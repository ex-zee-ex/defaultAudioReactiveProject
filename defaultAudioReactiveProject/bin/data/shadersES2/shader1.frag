precision highp float;

uniform sampler2D tex0;

varying vec2 texCoordVarying;

void main()
{
	vec4 color=vec4(0,0,0,0);
	
	color = texture2D(tex0, texCoordVarying);
	
	gl_FragColor = color;
}
