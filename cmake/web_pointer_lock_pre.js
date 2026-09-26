// HOUSE-03721: XNA exposes cursor visibility and mouse state, but no browser pointer-lock call.
// CNA/SDL reflects Game.IsMouseVisible on the canvas cursor style. Keep this tiny Web launcher
// bridge tied to that existing XNA state: menus never capture, and opening one releases the lock.
Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function () {
  const canvas = Module['canvas'];
  if (!canvas) return;

  canvas.addEventListener('click', function () {
    if (canvas.style.cursor !== 'none' || document.pointerLockElement === canvas) return;
    const request = canvas.requestPointerLock();
    if (request && request.catch) request.catch((error) => console.warn('Pointer lock denied:', error));
  });

  let pauseOnFocus = false;
  const releaseIfVisible = function () {
    if (canvas.style.cursor !== 'none') pauseOnFocus = false;
    if (canvas.style.cursor !== 'none' && document.pointerLockElement === canvas) {
      document.exitPointerLock();
    }
  };
  new MutationObserver(releaseIfVisible).observe(canvas, {attributes: true, attributeFilter: ['style']});

  // Browser Escape and tab switches can release pointer lock before SDL delivers a key/focus
  // event. Feed that loss into the existing XNA Escape binding while walking, so the pause menu
  // owns the cursor. A hidden tab waits until focus returns, when the game loop can sample it.
  const pauseIfWalking = function () {
    if (canvas.style.cursor !== 'none') return;
    const escape = {key: 'Escape', code: 'Escape', keyCode: 27, which: 27, bubbles: true};
    window.dispatchEvent(new KeyboardEvent('keydown', escape));
    window.setTimeout(function () {
      window.dispatchEvent(new KeyboardEvent('keyup', escape));
    }, 500);
  };
  document.addEventListener('pointerlockchange', function () {
    if (document.pointerLockElement === canvas || canvas.style.cursor !== 'none') return;
    pauseOnFocus = true;
    if (document.hasFocus()) pauseIfWalking();
  });
  window.addEventListener('blur', function () {
    if (document.pointerLockElement === canvas) document.exitPointerLock();
  });
  window.addEventListener('focus', function () {
    if (!pauseOnFocus) return;
    pauseOnFocus = false;
    pauseIfWalking();
  });
});
