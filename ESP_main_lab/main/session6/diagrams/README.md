# FSM diagrams

`fsm_initiator.png` and `fsm_responder.png` are generated from `fsm_diagrams.py`. Regenerate after changing the state machines:

```sh
python3 fsm_diagrams.py .
swift svg2png.swift fsm_initiator.svg fsm_initiator.png 1800 1330
swift svg2png.swift fsm_responder.svg fsm_responder.png 1900 1440
```

The `.svg` files scale without loss, which is handy for slides.
