open Webapi.Dom;

[%%bs.raw {|
  require('../../../src/style.less');
|}];

module RP = {
  [@bs.deriving jsConverter]
  type requestMethod = [ 
    | [@bs.as "GET"] `GET 
    | [@bs.as "POST"] `POST 
    | [@bs.as "PUT"] `PUT 
    | [@bs.as "DELETE"] `DELETE
  ];

  type request = {
    url: string,
    method: requestMethod
  };

  type internalRequest = {.
    "uri": string,
    "method": string
  };

  [@bs.module] external internalMakeRequest : internalRequest => Js.Promise.t('a) = "request-promise";

  let makeRequest = (request: request): Js.Promise.t('a) => {
    let payload = {
      "uri": request.url,
      "method": requestMethodToJs(request.method)
    };
    Js.log(payload);
    internalMakeRequest(payload);
  };
};

type user = {
  uid: string,
  username: string,
  score: int
};

[@bs.val] [@bs.scope "JSON"] external parseUsers : string => array(user) = "parse";

RP.makeRequest({ url: "http://127.0.0.1:8081/users", method: `GET }) |> Js.Promise.then_(value => {
  let parsed: array(user) = parseUsers(value);
  Js.log(parsed);
  Js.Promise.resolve(());
});

module Canvas = {
  [@bs.deriving abstract]
  type context = pri {
    mutable fillStyle: string,
    mutable strokeStyle: string
  };

  [@bs.send] external getContext2d: (Dom.element, [@bs.as "2d"] _) => context = "getContext";
  [@bs.send] external fillRectFloat: (context, float, float, float, float) => unit = "fillRect";
  [@bs.send] external fillRectInt: (context, int, int, int, int) => unit = "fillRect";
  [@bs.send] external drawImage: (context, Dom.element, int, int, int, int) => unit = "drawImage";

  [@bs.send] external clearRect: (context, int, int, int, int) => unit = "";
  [@bs.send] external strokeRect: (context, int, int, int, int) => unit = "";
};

module ArrayUtil = {
  let iterRange = (start: int, end_: int, arr: array('a), fn: ((int, 'a) => unit)): unit => {
    Belt.Array.slice(arr, start, end_ - start) |. Belt.Array.forEachWithIndex(fn);
  };
};

type world = {
  env: array(array(envElement)),
  heroes: list(hero),
  viewport: fourAxisElement(float),
  scene: scene,
  lastAnimationTime: option(int),
  recentKeys: list(string),
}
and hero = {
  color: string,
  position: fourAxisElement(float)
}
and envElement = {
  coordinate: int,
  elementType: elementType
}
and elementType = Floor | Stone | Active
and fourAxisElement('a) = {
  x: 'a,
  y: 'a,
  vx: 'a,
  vy: 'a
}
and scene = {
  ctx: Canvas.context,
  node: Dom.element,
  w: int,
  h: int,
  leftClicked: bool,
  rightClicked: bool,
  skyImage: option(Dom.element),
};

let canHeroJump = (hero: hero) => {
  hero.position.vy === 0.0;
}

let setRecentKeys = (world: world, newKey: string) => {
  let newRecent = List.append(world.recentKeys, [newKey]);
  let recentKeys = if (List.length(newRecent) > 10) {
    List.tl(newRecent);
  } else {
    newRecent;
  };
  {...world, recentKeys: recentKeys};
}

let didKonami = (recentKeys: list(string)) => {
  recentKeys == [
    "ArrowUp",
    "ArrowUp",
    "ArrowDown",
    "ArrowDown",
    "ArrowLeft",
    "ArrowRight",
    "ArrowLeft",
    "ArrowRight",
    "b",
    "a",
  ];
}

let setSkyImage = (world: world) => {
  let skyImage = if (world.scene.skyImage == None) {
    let pics = HtmlCollection.toArray(Document.getElementsByClassName("deadmeme", document));
    Some(pics[Random.int(Array.length(pics))]);
  } else {
    world.scene.skyImage;
  };
  {...world, scene: {...world.scene,
      skyImage: skyImage
  }};
}

let paintColor = (elementType: elementType): string => {
  switch(elementType) {
  | Floor => "#97FF29"
  | _     => "#aaaaaa"
  }
}

let stone = (coord : int): envElement => {
  let q = {coordinate: coord, elementType: Stone};
  q;
};

let generateRandomEnvironment = (length: int): array(array(envElement)) => {
  Random.self_init();
  let baseElement = {coordinate: 0, elementType: Floor};
  let m = Array.make_matrix(length, 20, baseElement);
  m[10][1] = stone(3);
  m[11][1] = stone(3);
  m[12][1] = stone(3);
  m[18][1] = stone(4);
  m[19][1] = stone(4);
  m[20][1] = stone(4);
  m[22][1] = stone(11);
  m[24][1] = stone(6);
  m[27][1] = stone(8);
  m;
};

let tileSize = 40.0;
let heroSize = tileSize /. 2.0;

let drawSky = (world: world): unit => {
  open Canvas;
  fillStyleSet(world.scene.ctx, "#008AC5");
  Canvas.fillRectInt(world.scene.ctx, 0, 0, world.scene.w, world.scene.h);
  switch (world.scene.skyImage) {
  | None => ();
  | Some(pic) => Canvas.drawImage(world.scene.ctx, pic, 250, 100, world.scene.w - 500, world.scene.h - 200);
  };
}

let constrain = (amt: float, low: float, high: float): float => {
  max(low, min(high, amt))
}

let constrainLeft = (amt: float, low: float): float => {
  max(low, amt)
}

let paint = (world: world): unit => {
  open Canvas;

  let envPaintStart = 0; /* TODO: Fix me */
  let xPaintOffset = world.viewport.x /. tileSize;
  let envPaintEnd = int_of_float((float_of_int(world.scene.w) +. world.viewport.x) /. tileSize) + 1;

  /* Draw the sky */
  drawSky(world);

  /* Draw the tiles */
  ArrayUtil.iterRange(envPaintStart, envPaintEnd, world.env, (idx, tiles) => {
    let paintIdx = idx - envPaintStart;
    Array.iter(tile => {
      let xOffset = switch (tile.elementType) {
      | Floor => 0.
      | _     => xPaintOffset
      };
      fillStyleSet(world.scene.ctx, paintColor(tile.elementType));
      world.scene.ctx |. Canvas.fillRectFloat(
        (float_of_int(paintIdx) -. xOffset) *. tileSize,
        float_of_int(world.scene.h) -. (
          float_of_int(tile.coordinate) *. tileSize -. world.viewport.y
        ),
        tileSize,
        tileSize);
    }, tiles);
  });

  /* Draw all heroes */
  List.iter(hero => {
    fillStyleSet(world.scene.ctx, hero.color);
    world.scene.ctx |. Canvas.fillRectFloat(
      hero.position.x -. xPaintOffset *. tileSize,
      float_of_int(world.scene.h) -. (hero.position.y -. world.viewport.y),
      heroSize,
      tileSize);
  }, world.heroes);
};

module Date = {
  type t;
  [@bs.new] external create: unit => t = "Date";
  [@bs.send] external getTime: t => int = "";
};

let findCollisions = (world: world, newX: float, newY: float):
  option((int, envElement)) => {
    let beginningTile = int_of_float(newX /. tileSize);
    let endTiles = if (newX /. tileSize -. float_of_int(beginningTile) > 0.5) {
      [beginningTile + 1];
    } else {
      [];
    }
    let columnsToCheck = List.append([beginningTile], endTiles);

    List.fold_left((collided, column) => {
      if (Js_option.isSome(collided)) {
        collided;
      } else {
        Array.fold_left((collided_, tile) => {
          if ((int_of_float(newY) <= (tile.coordinate + 1) * int_of_float(tileSize))  &&
              (int_of_float(newY) >= tile.coordinate * int_of_float(tileSize))) {
            Some((column, tile));
          } else {
            collided_;
          }
        }, None, Array.get(world.env, column));
      }
    }, None, columnsToCheck);
};

let dV = 0.005;
let vDamping = dV /. 10.0;
let maxV = 0.2;

let paintHero = (dT: float, world: world, hero: hero): world => {
  open Js_math;

  let newVY = hero.position.vy -. 0.00981;
  let leftDV = if (world.scene.leftClicked) {
    dT *. dV;
  } else {
    0.0;
  };
  let rightDV = if (world.scene.rightClicked) {
    dT *. dV;
  } else {
    0.0;
  };
  let vX' = hero.position.vx +. rightDV -. leftDV;
  let vX'' = -1.0 *. sign_float(vX') *. vDamping *. dT +. /* V damping */
    if (abs_float(vX') > maxV) { sign_float(vX') *. maxV } else { vX' };
  let newVX = if (abs_float(vX'') <= vDamping *. dT) { 0.0 } else { vX'' };
  let newViewPortY = if (hero.position.y > 200.0) { world.viewport.y +. newVY *. dT } else {world.viewport.y};
  let magicViewportNumber = 2.0175;
  let newViewPortX = if (hero.position.x > 200.0) { 
    constrainLeft(world.viewport.x +. newVX *. dT /. magicViewportNumber, 0.) 
  } else {
    world.viewport.x
  };
  let newY = hero.position.y +. newVY *. dT;

  let boundLeft  = world.viewport.x;
  let boundRight = float_of_int(world.scene.w) -. heroSize +. world.viewport.x;
  let newX = constrain(hero.position.x +. newVX *. dT, boundLeft, boundRight);

  /* detect collisions */
  switch (findCollisions(world, newX, newY)) {
    | None => {...world, heroes: List.append(world.heroes, [{...hero, 
    position: {
      x: newX,
      y: newY,
      vx: newVX,
      vy: newVY
    }}])};
    | Some((_, collidedTile)) => {
      {...world, heroes: List.append(world.heroes, [{...hero,
      position: {
        x: newX,
        y: float_of_int(collidedTile.coordinate + 1) *. tileSize,
        vx: newVX,
        vy: 0.0
      }}]),
      viewport: {
        x: newViewPortX,
        y: newViewPortY,
        vx: 0.0,
        vy: 0.0
    }};
    };
  };
};

let step = (world: world): world => {
  let currentTime = Date.create() |> Date.getTime;

  let dT = switch(world.lastAnimationTime) {
    | None => 1000.0 /. 60.0;
    | Some(time) => float_of_int(currentTime - time);
  };

  /* TODO: make nicer */
  List.fold_left(paintHero(dT), 
    {...world, heroes: []}, world.heroes);
};

let mainLoop = (world: world) => {
  let currentWorld = ref(world);

  HtmlElement.focus(Element.unsafeAsHtmlElement(world.scene.node));
  Element.addKeyDownEventListener((e) => {
    open KeyboardEvent;

    /* if key up jump, if right, keep adding speed until max */
    /* TODO: use some kind of smart typesafe collection */
    if (key(e) === "ArrowUp") {
      currentWorld := List.fold_left((world, hero) => {
        if (canHeroJump(hero)) {
          let jumpingHero = {...hero, position: {...hero.position,
            vy: 0.4
          }};

          {...world, heroes: List.append(world.heroes, [jumpingHero])};
        } else {
          {...world, heroes: List.append(world.heroes, [hero])};
        }
      }, {...currentWorld^, heroes: []}, currentWorld^.heroes);
    } else if (key(e) === "ArrowRight") {
      currentWorld := {...currentWorld^, scene: {...currentWorld^.scene,
        rightClicked: true,
        leftClicked: false
      }};
    } else if (key(e) === "ArrowLeft") {
      currentWorld := {...currentWorld^, scene: {...currentWorld^.scene,
        leftClicked: true,
        rightClicked: false
      }};
    };
  }, world.scene.node);

  Element.addKeyUpEventListener((e) => {
    open KeyboardEvent;

    currentWorld := setRecentKeys(currentWorld^, key(e));

    if (didKonami(currentWorld^.recentKeys)) {
      currentWorld := setSkyImage(currentWorld^);
    };

    if (key(e) === "ArrowRight") {
      currentWorld := {...currentWorld^, scene: {...currentWorld^.scene,
        rightClicked: false
      }};
    } else if (key(e) === "ArrowLeft") {
      currentWorld := {...currentWorld^, scene: {...currentWorld^.scene,
        leftClicked: false
      }};
    };
  }, world.scene.node);

  let rec requestFrame = () => {
    Webapi.requestAnimationFrame((_) => {
      let newWorld = step(currentWorld^);
      paint(newWorld);
      currentWorld := newWorld;

      requestFrame();
    });
  };

  requestFrame();
};

let initialize = (): option(world) => {
  switch (Document.getElementById("scene", document)) {
    | None => None;
    | Some(scene) => {
      let canvasHeight = Element.clientHeight(scene);
      let canvasWidth = Element.clientWidth(scene);

      scene |> Element.setAttribute("height", string_of_int(canvasHeight) ++ "px");
      scene |> Element.setAttribute("width", string_of_int(canvasWidth) ++ "px");

      Some({
        env: generateRandomEnvironment(80),
        heroes: [{
          color: "#00deff",
          position:{x: 80.0, y: 80.0, vx: 0.0, vy: 0.0}
        }, {
          color: "#ee0000",
          position:{x: 120.0, y: 80.0, vx: 0.0, vy: 0.0}
        }],
        viewport: {x: 0.0, y: -80.0, vx: 0.0, vy: 0.0},
        scene: {
          ctx: scene |. Canvas.getContext2d,
          node: scene,
          w: canvasWidth,
          h: canvasHeight,
          leftClicked: false,
          rightClicked: false,
          skyImage: None
        },
        lastAnimationTime: None,
        recentKeys: []
      });
    }
  };
};

switch (initialize()) {
  | None => Js.log("initialize failed");
  | Some(world) => mainLoop(world);
};
