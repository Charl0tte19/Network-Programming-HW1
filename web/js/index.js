var s = skrollr.init();

$(".list1").mouseenter(
  function(){
     $(".name").css("display","none");
  }
);
$(".list3").mouseenter(
  function(){
     $(".name").css("display","none");
  }
);


function type(target,second,delay){
  console.log(second);
  setTimeout(function(){$(target).css("animation","typing "+second+" linear");
$(target).css("opacity","1");},delay);    
}

$(".screen").mouseenter(
  function(){
    $(".dialog").css("opacity","1");
    $(".sylvie").css("opacity","1");
    type(".d2","2s","1000");
  }
);



var canvas = document.getElementById("canvas");
var ctx = canvas.getContext("2d");
var particlesOnScreen = 245;
var particlesArray = [];
var w,h;
w = canvas.width = window.innerWidth;
h = canvas.height = 1700;

function random(min, max) {
    return min + Math.random() * (max - min + 1);
};

function clientResize(ev){
    w = canvas.width = window.innerWidth;
    h = canvas.height = 1700;
};
window.addEventListener("resize", clientResize);

function createSnowFlakes() {
    for(var i = 0; i < particlesOnScreen; i++){
        particlesArray.push({
            x: Math.random() * w,  
            y: Math.random() * h,  
            opacity: Math.random(),  
            speedX: random(-11, 11),  
            speedY: random(7, 15),    
            radius:random(0.5, 4.2),
        })
    }
};

function drawSnowFlakes(){
    for(var i = 0; i < particlesArray.length; i++){    
        var gradient = ctx.createRadialGradient(  
            particlesArray[i].x,   
            particlesArray[i].y,   
            0,                     
            particlesArray[i].x,   
            particlesArray[i].y,   
            particlesArray[i].radius  
            );

            gradient.addColorStop(0, "rgba(255, 255, 255," + particlesArray[i].opacity + ")");  // white
            gradient.addColorStop(.8, "rgba(210, 236, 242," + particlesArray[i].opacity + ")");  // bluish

 

            gradient.addColorStop(1, "rgba(237, 247, 249," + particlesArray[i].opacity + ")");   // lighter bluish
          
            ctx.beginPath(); 
            ctx.arc(
            particlesArray[i].x,  
            particlesArray[i].y,  
            particlesArray[i].radius,  
            0,                         
            Math.PI*2,                 
            false                     
            );

        ctx.fillStyle = gradient;   
        ctx.fill();                 
    }
};

function moveSnowFlakes(){
    for (var i = 0; i < particlesArray.length; i++) {
        particlesArray[i].x += particlesArray[i].speedX;     
        particlesArray[i].y += particlesArray[i].speedY;     

        if (particlesArray[i].y > h) {                                                                               
            particlesArray[i].x = Math.random() * w * 1.5;
            particlesArray[i].y = -50;
        }
    }
};

function updateSnowFall  () {
    ctx.clearRect(0, 0, w, h);
    drawSnowFlakes();
    moveSnowFlakes();
};

setInterval(updateSnowFall,50);
createSnowFlakes();