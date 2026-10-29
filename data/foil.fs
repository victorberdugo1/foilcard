#version 330

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform sampler2D background;
uniform sampler2D character;
uniform sampler2D mask;
uniform sampler2D cardBack;
uniform vec2 tilt;
uniform float time;
uniform float burn;
uniform float rarity;
uniform float glare;
uniform float diffraction;
uniform float tera;
uniform float edgeOn;
uniform vec3 layerMap;
uniform vec4 effects;

out vec4 finalColor;

vec3 rainbow(float t)
{
    return 0.5 + 0.5*cos(6.28318*(t + vec3(0.0, 0.33, 0.67)));
}

vec2 gradientHash(vec2 p)
{
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return -1.0 + 2.0*fract(sin(p)*43758.5453123);
}

float gradientNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f*f*(3.0 - 2.0*f);
    return mix(mix(dot(gradientHash(i), f), dot(gradientHash(i + vec2(1, 0)), f - vec2(1, 0)), u.x),
               mix(dot(gradientHash(i + vec2(0, 1)), f - vec2(0, 1)), dot(gradientHash(i + vec2(1, 1)), f - vec2(1, 1)), u.x), u.y);
}

float fbm(vec2 p)
{
    mat2 rotation = mat2(0.8, 0.6, -0.6, 0.8);
    float sum = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; i++) {
        sum += amplitude*gradientNoise(p);
        p = rotation*p*2.03;
        amplitude *= 0.5;
    }
    return 0.5 + sum;
}

vec2 hash22(vec2 p)
{
    vec3 q = fract(vec3(p.xyx)*vec3(0.1031, 0.103, 0.0973));
    q += dot(q, q.yzx + 33.33);
    return fract((q.xx + q.yz)*q.zy);
}

vec4 ringAverage(sampler2D tex, vec2 uv, float radius, int count)
{
    vec4 sum = vec4(0.0);
    for (int i = 0; i < count; i++) {
        float angle = 6.28318*float(i)/float(count);
        sum += texture(tex, uv + radius*vec2(cos(angle), sin(angle)));
    }
    return sum/float(count);
}

float faceShade(vec2 uv)
{
    return 1.0 - 0.7*dot(tilt, uv - 0.5);
}

float tiltGlint(vec2 uv)
{
    float axis = dot(uv - 0.5, vec2(0.8, -0.6)) - dot(tilt, vec2(0.55, 0.35));
    return pow(max(0.0, 1.0 - abs(axis)*3.5), 3.0);
}

float edgeGlow(vec2 uv)
{
    float coverage = ringAverage(texture0, uv, 0.015, 8).a;
    return pow(4.0*coverage*(1.0 - coverage), 4.0);
}

float sparkleLayer(vec2 uv)
{
    if (effects.y <= 0.0) return 0.0;
    mat2 rotation = mat2(0.866, 0.5, -0.5, 0.866);
    vec2 p = rotation*(uv*(3.0 + 1.5*rarity)) + tilt*0.8 + vec2(time*0.25, -time*0.18);
    float seed = fract(sin(dot(floor(p), vec2(12.9898, 78.233)))*43758.5453);
    float glint = step(0.7, seed)*(1.0 - smoothstep(0.0, 0.12, length(fract(p) - 0.5)));
    float shimmer = 0.5 + 0.5*sin(time*5.0 + 6.28318*gradientNoise(uv*4.0));
    return glint*shimmer;
}

vec3 prismEdge(vec2 uv)
{
    if (effects.w <= 0.0) return vec3(0.0);
    vec3 prism = mix(rainbow(uv.y*1.4 - time*0.3 + tilt.x*0.5), vec3(0.45, 1.0, 0.75), 0.35);
    float spine = pow(max(0.0, 1.0 - 2.0*abs(uv.y - 0.5)), 1.5);
    float sides = smoothstep(0.15, 0.33, abs(uv.x - 0.5));
    float tips = smoothstep(0.3, 0.42, abs(uv.y - 0.5))*(1.0 - smoothstep(0.0, 0.1, abs(uv.x - 0.5)));
    float slide = edgeOn*edgeOn;
    float sliver = smoothstep(0.85, 1.0, edgeOn);
    float rim = edgeGlow(uv)*(sides*(0.15 + 1.8*slide)*(0.3 + 0.7*spine) + tips*(0.3 + 2.2*slide));
    return prism*(rim + 1.6*sliver*spine);
}

float sweepLight(vec2 uv)
{
    float axis = dot(uv, vec2(0.7, -0.7)) + tilt.x*0.3;
    float center = fract(time*0.6)*1.8 - 0.9;
    return 1.0 - smoothstep(0.0, 0.08, abs(axis - center));
}

vec4 groundLayer(vec2 uv)
{
    vec2 p = uv + tilt*0.045;
    vec2 strip = vec2(0.5 + (p.x - 0.5)*0.8, 0.895 + 0.105*(p.y - 0.7)/0.17);
    float inStrip = step(0.895, strip.y)*step(strip.y, 1.0);
    vec4 texel = texture(character, strip);
    return vec4(texel.rgb, texel.a*inStrip);
}

const vec3 JEWELS[3] = vec3[3](vec3(0.08, 0.5, 1.0), vec3(1.0, 0.1, 0.32), vec3(1.0, 0.7, 0.1));

vec3 jewelTint()
{
    float k = fract(time*0.05)*3.0;
    int i = int(k);
    return mix(JEWELS[i], JEWELS[(i + 1) % 3], smoothstep(0.6, 1.0, fract(k)));
}

vec2 facetCell(vec2 p, out float edge, out vec2 offset)
{
    vec2 cell = floor(p);
    vec2 f = fract(p);
    float nearest = 8.0;
    float second = 8.0;
    vec2 id = cell;
    offset = vec2(0.0);
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 neighbor = cell + vec2(x, y);
            vec2 r = vec2(x, y) + hash22(neighbor) - f;
            float d = dot(r, r);
            if (d < nearest) {
                second = nearest;
                nearest = d;
                id = neighbor;
                offset = -r;
            } else if (d < second) {
                second = d;
            }
        }
    }
    edge = sqrt(second) - sqrt(nearest);
    return id;
}

float teraFront(vec2 uv)
{
    return uv.y + 0.06*sin(uv.x*11.0 + uv.y*5.0) + 0.04*sin(uv.x*23.0 - uv.y*13.0);
}

float teraThreshold()
{
    return mix(1.15, -0.15, tera);
}

float teraMask(float front)
{
    return smoothstep(-0.02, 0.02, front - teraThreshold());
}

vec3 crystalize(vec2 uv, vec4 texel)
{
    vec3 color = texel.rgb;
    if (tera <= 0.0 || texel.a <= 0.0) return color;
    float front = teraFront(uv);
    float band = front - teraThreshold();
    if (band < -0.15) return color;

    vec3 tint = jewelTint();
    vec3 light = normalize(vec3(vec2(0.35, -0.5) - 0.9*tilt, 1.0));
    float edge;
    vec2 offset;
    vec2 id = facetCell(uv*10.0, edge, offset);
    vec2 slopeSeed = hash22(id + 11.0);
    vec2 glintSeed = hash22(id + 37.0);
    float bevel = 1.0 - smoothstep(0.0, 0.35, edge);
    vec3 normal = normalize(vec3(2.2*(slopeSeed - 0.5) + 2.0*offset*bevel, 0.8));
    float diffuse = max(dot(normal, light), 0.0);
    float specular = pow(max(dot(normal, normalize(light + vec3(0.0, 0.0, 1.0))), 0.0), 48.0);
    float twinkle = pow(0.5 + 0.5*sin(time*2.0 + 6.28318*glintSeed.x), 14.0);
    vec3 irid = rainbow(dot(normal.xy, vec2(1.3, 0.7)) + 0.25*glintSeed.y + dot(tilt, vec2(0.5, 0.35)));

    float innerEdge;
    vec2 innerOffset;
    vec2 innerId = facetCell(uv*6.0 + 2.0*tilt + 1.2*normal.xy + vec2(7.3, 2.1), innerEdge, innerOffset);
    vec2 innerSeed = hash22(innerId + 53.0);
    float innerBevel = 1.0 - smoothstep(0.0, 0.4, innerEdge);
    vec3 innerNormal = normalize(vec3(2.4*(innerSeed - 0.5) + 1.6*innerOffset*innerBevel, 0.7));
    float innerLight = max(dot(innerNormal, normalize(vec3(vec2(-0.4, 0.3) + 0.9*tilt, 1.0))), 0.0);
    vec3 interior = tint*(0.15 + 1.3*innerLight*innerLight)
        + 0.3*rainbow(innerSeed.x + dot(tilt, vec2(0.4, 0.3)))*innerBevel
        + 0.3*tint*(1.0 - smoothstep(0.0, 0.05, innerEdge));

    float shade = 0.55 + 0.9*dot(color, vec3(0.299, 0.587, 0.114));
    vec3 crystal = interior*shade*(0.45 + 0.75*diffuse);
    crystal += irid*(0.1 + 0.55*(1.0 - normal.z))*(0.4 + diffuse);
    crystal += vec3(1.0, 0.97, 0.9)*1.6*specular + mix(irid, vec3(1.0), 0.5)*0.45*twinkle;
    crystal += mix(tint, vec3(1.0), 0.6)*0.5*(1.0 - smoothstep(0.0, 0.04, edge));

    float flash = exp(-band*band/0.0025)*sin(3.14159*tera);
    return mix(color, crystal, teraMask(front)) + flash*(0.8 + 0.5*rainbow(front*5.0 - time*0.5));
}

vec3 teraTint()
{
    return mix(jewelTint(), vec3(1.0), 0.45);
}

float teraAura(vec2 uv, float alpha)
{
    if (tera <= 0.0) return 0.0;
    float mask = teraMask(teraFront(uv));
    if (mask <= 0.0) return 0.0;
    float coverage = ringAverage(character, uv, 0.008, 6).a;
    float edge = pow(4.0*coverage*(1.0 - coverage), 3.0);
    float halo = 0.2*coverage*(1.0 - alpha);
    return (0.8*edge + halo)*mask;
}

vec3 frontColor(vec2 uv, float inside, vec4 frame)
{
    float tier = rarity*0.5;
    vec3 sky = texture(background, uv + tilt*0.1).rgb;
    vec4 ground = groundLayer(uv);
    vec2 figureUV = uv - tilt*0.035;
    vec4 figure = texture(character, figureUV);
    vec3 art = mix(mix(sky, ground.rgb, ground.a), crystalize(figureUV, figure), figure.a);
    art += teraTint()*teraAura(figureUV, figure.a);

    vec3 foil = rainbow(dot(uv, vec2(2.0, 1.0)) + dot(tilt, vec2(1.5, 1.0)));
    float shine = pow(max(0.0, 1.0 - 2.0*length(uv - 0.5 - 0.5*tilt)), 3.0);

    vec2 cell = uv*18.0 + tilt*2.0;
    float diamonds = 0.5 + 0.25*(cos(cell.x*3.1416) + cos(cell.y*3.1416));
    vec3 holo = rainbow(diamonds + time*0.1 + dot(tilt, vec2(0.7, 0.4)));

    vec3 rim = frame.rgb*mix(vec3(1.0), (0.6 + 0.4*tier)*1.3*foil, 0.35*effects.w) + 0.4*shine*effects.w;

    vec3 face = mix(art, art*holo*(1.2 + 0.2*tier), (0.15 + 0.15*tier)*effects.z);
    face += 0.2*shine*foil;
    face += effects.z*(0.3 + 0.2*tier)*length(tilt)*tiltGlint(uv)*mix(vec3(1.0), foil, 0.5);
    return mix(rim, face, inside)*faceShade(uv);
}

vec3 backColor(vec2 uv)
{
    float tier = rarity*0.5;
    vec2 p = vec2(1.0 - uv.x, uv.y);
    float c = texture(cardBack, p).r;
    float near = ringAverage(cardBack, p, 0.006, 8).r;
    float mid = ringAverage(cardBack, p, 0.016, 8).r;
    float far = ringAverage(cardBack, p, 0.04, 8).r;
    float solid = smoothstep(0.9, 1.0, near)*smoothstep(0.9, 1.0, c);
    float white = smoothstep(0.3, 0.85, c);

    float wave = 0.5 + 0.5*sin(dot(uv, vec2(7.0, 5.0)) + dot(tilt, vec2(5.0, 3.0)) - time*0.8);
    vec3 gold = mix(vec3(1.0, 0.5, 0.08), vec3(1.0, 0.92, 0.55), wave);
    vec3 edgeColor = mix(vec3(0.7, 0.3, 1.0), vec3(1.0, 0.8, 0.25), 0.5 + 0.5*sin(time*1.5 + tilt.x*3.0));
    vec3 neon = mix(edgeColor, gold, 0.5 + 0.5*sin(time*1.5 + 4.0*uv.y + 3.0*tilt.x));
    float radial = 1.0 - smoothstep(0.0, 0.7, length(uv - 0.5 - 0.1*tilt));
    vec3 base = vec3(0.012, 0.01, 0.016) + 0.07*radial*vec3(0.9, 0.5, 0.15);

    float band = 0.5 + 0.5*sin(dot(uv, vec2(9.0, 6.0)) + dot(tilt, vec2(7.0, 4.0)) - time*1.2);
    float brushed = 0.5 + 0.5*sin(dot(uv, vec2(60.0, -40.0)) + dot(tilt, vec2(14.0, 9.0)));
    vec3 metal = mix(vec3(0.62, 0.34, 0.05), vec3(1.0, 0.72, 0.18), smoothstep(0.0, 0.55, band));
    metal = mix(metal, vec3(1.0, 0.94, 0.62), smoothstep(0.55, 1.0, band)*(0.6 + 0.4*brushed));
    float tintSweep = 0.5 + 0.5*sin(dot(uv, vec2(3.0, 2.0)) - dot(tilt, vec2(3.0, 2.0)) + time*0.9);
    metal = mix(metal, edgeColor*1.25, 0.4*tintSweep*(0.4 + 0.6*length(tilt)));

    vec3 color = base + white*metal*(1.15 - 0.2*solid);
    color += (1.0 - white)*(0.35*far*neon + 0.9*mid*mid*mix(gold, edgeColor, 0.35));

    float sheen = pow(max(0.0, sin(uv.x*4.0 + uv.y*3.0 - time*3.0 + tilt.x*3.0)), 8.0);
    color += effects.z*(0.8 + 0.6*tier)*sheen*white*vec3(1.0, 0.9, 0.6);
    color += effects.w*(0.3 + 0.9*length(tilt))*white*(1.0 - near)*edgeColor;

    vec2 grid = uv*28.0;
    float h = fract(sin(dot(floor(grid), vec2(12.9898, 78.233)))*43758.5453);
    vec2 f = fract(grid) - 0.5;
    vec2 q = abs(f);
    float cross = (1.0 - smoothstep(0.0, 0.05, min(q.x, q.y)))*(1.0 - smoothstep(0.0, 0.4, max(q.x, q.y)));
    float twinkle = step(0.9, h)*(0.6*smoothstep(0.25, 0.0, length(f)) + cross)*(0.5 + 0.5*sin(time*3.0 + 40.0*h));
    color += effects.y*1.5*twinkle*mix(vec3(1.0), gold, 0.6);
    return color;
}

float flare(vec2 p, float wing, float size, float shift)
{
    float angle = atan(p.x, p.y);
    float fanOffset = (angle - wing - shift)/0.32;
    float upOffset = angle/0.14;
    float downOffset = (3.14159 - angle)/0.3;
    float rays = exp(-fanOffset*fanOffset) + exp(-upOffset*upOffset) + 0.45*exp(-downOffset*downOffset);
    return rays*exp(-length(p)/size);
}

vec3 cellGlint(vec2 grid, vec2 cell)
{
    vec2 seed = hash22(cell);
    vec2 phaseSeed = hash22(cell + 17.0);
    vec2 jitter = hash22(cell + 31.0) - 0.5;
    vec2 p = grid - cell - 0.5 - 0.5*jitter;
    if (dot(p, p) >= 1.0) return vec3(0.0);

    float phase = time*(0.6 + 1.4*seed.y) + 6.28318*phaseSeed.x + dot(tilt, vec2(4.0, 3.0))*(0.5 + seed.x);
    float blink = pow(0.5 + 0.5*sin(phase), 6.0);
    if (blink < 0.004) return vec3(0.0);

    p.x = abs(p.x) + 0.0001;
    p.y *= mix(1.0, -1.0, step(0.5, phaseSeed.y));
    float size = (0.16 + 0.16*seed.x)*(0.6 + 0.4*blink);
    float wing = 0.6 + 0.9*phaseSeed.x;

    vec3 spectrum = vec3(flare(p, wing, size*1.3, -0.24), flare(p, wing, size, 0.0), flare(p, wing, size*0.75, 0.24));
    vec3 tint = mix(vec3(1.0), mix(vec3(1.0, 0.78, 0.5), vec3(0.6, 0.8, 1.0), step(0.5, seed.y)), 0.6);
    float core = exp(-dot(p, p)*160.0/(0.5 + 4.0*size)) + 0.35*exp(-length(p)/(0.35*size));
    float window = 1.0 - smoothstep(0.7, 1.0, length(p));
    return (1.6*spectrum*tint + 1.2*core)*blink*window*(0.5 + phaseSeed.x);
}

vec3 diffractionGlints(vec2 uv)
{
    if (diffraction <= 0.0) return vec3(0.0);
    vec2 grid = uv*7.0;
    vec2 cell = floor(grid);
    vec3 sum = vec3(0.0);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
            sum += cellGlint(grid, cell + vec2(x, y));
    return sum;
}

vec3 burnColor(vec2 uv, vec3 color, float weight, out float holes)
{
    if (burn <= 0.0) {
        holes = 0.0;
        return color;
    }
    float field = smoothstep(0.25, 0.75, fbm(uv*4.0));
    float d = field - mix(-0.1, 1.1, burn);
    float charred = step(d, 0.0);
    float glow = step(0.0, d)*(1.0 - smoothstep(0.0, 0.06, d));
    float flicker = 0.9 + 0.1*sin(time*16.0 + 40.0*field);
    vec3 emberColor = mix(vec3(1.0, 0.9, 0.5), vec3(1.0, 0.35, 0.05), smoothstep(0.0, 0.06, d));
    vec3 ash = vec3(0.07, 0.035, 0.02);
    holes = step(d, -0.03);
    return mix(color, ash, charred*weight) + emberColor*glow*flicker*weight;
}

vec3 mirrorSheen(vec2 uv, vec3 base)
{
    if (glare <= 0.0) return base;
    vec2 p = uv*26.0;
    p.x += 0.5*mod(floor(p.y), 2.0);
    vec2 f = fract(p) - 0.5;
    float dome = 1.0 - smoothstep(0.38, 0.5, length(f));
    float facet = 0.5 + 0.5*clamp(dot(f*2.0, vec2(0.7, -0.7)) + dot(tilt, vec2(1.2, 0.8)), -1.0, 1.0);

    float axis = dot(uv - 0.5, vec2(0.8, -0.6)) + dot(tilt, vec2(0.7, 0.45));
    float sweep = exp(-axis*axis*10.0);
    vec2 spot = tilt - vec2(0.55, -0.35);
    float flash = 0.45*exp(-dot(spot, spot)*4.0);

    vec3 irid = mix(vec3(0.75, 0.45, 1.0), vec3(1.0, 0.6, 0.9), 0.5 + 0.5*sin(axis*6.0));
    vec3 sheen = base + irid*(0.25 + 0.75*facet)*dome*(0.3 + sweep)*0.6 + irid*sweep*0.6;
    return mix(sheen, mix(sheen, vec3(1.0), 0.5)*1.1, flash);
}

void main()
{
    vec2 uv = fragTexCoord;
    float holes;
    if (layerMap.z > 0.0) {
        vec4 texel = texture(texture0, uv);
        vec3 crystal = crystalize(uv, texel);
        float aura = teraAura(uv, texel.a);
        vec3 layerColor = mix(crystal, mirrorSheen(uv, crystal), glare) + teraTint()*aura;
        layerColor = burnColor(layerMap.xy + layerMap.z*uv, layerColor, 1.0, holes);
        float layerAlpha = max(texel.a, clamp(aura, 0.0, 1.0))*(1.0 - holes);
        if (layerAlpha <= 0.0) discard;
        finalColor = vec4(layerColor, layerAlpha);
        return;
    }

    vec4 frame = texture(texture0, uv);
    float inside = texture(mask, uv).r;
    float body = max(frame.a, inside);
    float tier = rarity*0.5;

    vec3 neon = mix(vec3(1.0, 0.8, 0.2), vec3(0.6, 0.2, 1.0), 0.5 + 0.5*tilt.x);
    vec3 card;
    if (gl_FrontFacing) card = frontColor(uv, inside, frame);
    else card = backColor(uv);

    vec3 color = mix(neon, card, body);
    color = mix(color, mirrorSheen(uv, color), glare*body);
    color += effects.z*(0.25 + 0.15*tier)*sweepLight(uv)*vec3(1.0, 0.95, 0.85)*body;
    color += effects.w*prismEdge(uv)*body;
    color += 0.6*effects.y*sparkleLayer(uv)*body;
    color += diffraction*diffractionGlints(uv)*body;
    color = burnColor(uv, color, body, holes);

    float aura = 0.0;
    if (effects.x > 0.0) {
        float pulse = 0.7 + 0.3*sin(time*3.0);
        float glowReach = 0.6*ringAverage(texture0, uv, 0.035, 12).a + 0.4*ringAverage(texture0, uv, 0.08, 12).a*(0.4 + 1.2*length(tilt));
        aura = (1.0 - body)*glowReach*pulse*(1.0 - burn)*effects.x;
    }
    float alpha = max(body*(1.0 - holes), aura);
    if (alpha <= 0.0) discard;

    finalColor = vec4(color, alpha);
}
