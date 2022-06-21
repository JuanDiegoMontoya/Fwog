#ifndef FROG_H
#define FROG_H

// All the code in this file (including the Froge SDF) is by Clement Pirelli
// Taken from this shader: https://www.shadertoy.com/view/WstGDs

struct sdfRet
{
    float sdf;
    float id;
};

struct sphere
{
    vec3 c;
    float r;
};

struct ray
{
    vec3 origin;
    vec3 direction;
};

struct dirLight
{
    vec3 direction;
    float intensity;
    vec3 color;
    float ambient;
};


// ----distance functions----


float sphDist(vec3 p, sphere s)
{
    return distance(p, s.c) - s.r;
}

float sdCapsule( vec3 p, vec3 a, vec3 b, float r )
{
    vec3 pa = p - a, ba = b - a;
    float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
    return length( pa - ba*h ) - r;
}

//from iq : https://iquilezles.org/articles/distfunctions
float smin( float d1, float d2, float k ) 
{
    float h = clamp( 0.5 + 0.5*(d2-d1)/k, 0.0, 1.0 );
    return mix( d2, d1, h ) - k*h*(1.0-h); 
}

//from iq : https://iquilezles.org/articles/distfunctions
float smax( float d1, float d2, float k ) 
{
    float h = clamp( 0.5 - 0.5*(d2+d1)/k, 0.0, 1.0 );
    return mix( d2, -d1, h ) + k*h*(1.0-h); 
}


sdfRet frog(vec3 point)
{
    //body
    float id = .0;
    float dist = sphDist(point, sphere(vec3(.0,.05,.0), .25 ));
    
    //shoulders
    dist = smin(dist, sphDist(point, sphere(vec3(.34,-.12,-.1), .08)), .2);
    
    ////lower body
    dist = smin(dist, sphDist(point, sphere(vec3(.0,.02,.3), .1)), .3);
    
    ////thighs
    dist = smin(dist, sphDist(point, sphere(vec3(.24,-.12, .34), .08)), .2);
    
    //head
    dist = smin(dist, sphDist(point, sphere(vec3(.0,.04,-.25), .22)), .1);
    
    dist = max(-max(sdCapsule(point, vec3(-1.,-.04,-.5),vec3(1.,-.04,-.5),.1),point.y-.01),dist);
    
    //eyes
    float distEyes = sphDist(point, sphere(vec3(.15,.11,-.3), .14));
    
    if(dist > distEyes) id = 2.0;
    dist = min(dist,distEyes);
    
    //iris
    float distIris = sphDist(point, sphere(vec3(.19,.11,-.32), .1));
    if(dist > distIris) id = 3.0;
    dist = min(dist,distIris);
    
    return sdfRet(dist, id);
}

sdfRet map(vec3 point)
{
    
    point.z *=-1.0;
    point = vec3(abs(point.x),point.y,point.z);
    
    sdfRet d = frog(point);
    
    return d;
}


vec3 idtocol(float id)
{
    vec3 col = vec3(.2,.9,.2);
    
    if(id > .5) col = vec3(.1,.1,.6);
    if(id > 1.5) col = vec3(1.0);
    if(id > 2.5) col = vec3(.1);
    
    return col;
}

#endif // FROG_H