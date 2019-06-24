Shader "SubstanceManagerPBR"
{
    Properties
    {
        _Color ("Color", Color) = (1,1,1,1)
        _RGBM ("RGBM", 2D) = "white" {}
		_NRMS ("NRMS", 2D) = "white" {}
		_normalPower("Normal Strength", Range(0,1)) = 1
    }
    SubShader
    {
        Tags { "RenderType"="Opaque" }
        LOD 200

        CGPROGRAM
        // Physically based Standard lighting model, and enable shadows on all light types
        #pragma surface surf Standard fullforwardshadows

        // Use shader model 3.0 target, to get nicer looking lighting
        #pragma target 3.0

        sampler2D _RGBM, _NRMS;

        struct Input
        {
            float2 uv_RGBM;
        };

		fixed _normalPower;
        fixed4 _Color;

        void surf (Input IN, inout SurfaceOutputStandard o)
        {
            // Albedo comes from a texture tinted by color
            fixed4 c = tex2D (_RGBM, IN.uv_RGBM);
			fixed4 d = tex2D (_NRMS, IN.uv_RGBM);
            o.Albedo = c.rgb * _Color;
			fixed3 n = (d.rgb*2 - 1) * fixed3(_normalPower, _normalPower, 1);

			o.Normal = n; // (d.rgb * 2.00f - 1.0f) * 3;
            o.Metallic = c.a;
            o.Smoothness = 1-d.a;
        }
        ENDCG
    }
    FallBack "Diffuse"
}
