import { useEffect, useRef, useState } from "react";
import { getSliderState, getComboBoxState, getToggleState } from "../juceBridge";

export function useSliderParam(id: string, defaultValue = 0) {
  const stateRef = useRef(getSliderState(id));
  const [value, setValue] = useState(defaultValue);
  const [normalised, setNormalised] = useState(0);

  useEffect(() => {
    const state = stateRef.current;
    const sync = () => {
      setValue(state.getScaledValue());
      setNormalised(state.getNormalisedValue());
    };
    const valHandle = state.valueChangedEvent.addListener(sync);
    const propHandle = state.propertiesChangedEvent.addListener(sync);
    sync();
    return () => {
      state.valueChangedEvent.removeListener(valHandle);
      state.propertiesChangedEvent.removeListener(propHandle);
    };
  }, []);

  return {
    value,
    normalised,
    setNormalised: (v: number) => stateRef.current.setNormalisedValue(v),
    dragStart: () => stateRef.current.sliderDragStarted(),
    dragEnd: () => stateRef.current.sliderDragEnded(),
  };
}

export function useComboBoxParam(id: string, defaultIndex = 0) {
  const stateRef = useRef(getComboBoxState(id));
  const [index, setIndex] = useState(defaultIndex);
  const [choices, setChoices] = useState<string[]>([]);

  useEffect(() => {
    const state = stateRef.current;
    const sync = () => {
      setIndex(state.getChoiceIndex());
      setChoices([...state.properties.choices]);
    };
    const valHandle = state.valueChangedEvent.addListener(sync);
    const propHandle = state.propertiesChangedEvent.addListener(sync);
    sync();
    return () => {
      state.valueChangedEvent.removeListener(valHandle);
      state.propertiesChangedEvent.removeListener(propHandle);
    };
  }, []);

  return {
    index,
    choices,
    setIndex: (i: number) => stateRef.current.setChoiceIndex(i),
  };
}

export function useToggleParam(id: string, defaultValue = false) {
  const stateRef = useRef(getToggleState(id));
  const [value, setValue] = useState(defaultValue);

  useEffect(() => {
    const state = stateRef.current;
    const sync = () => setValue(state.getValue());
    const valHandle = state.valueChangedEvent.addListener(sync);
    const propHandle = state.propertiesChangedEvent.addListener(sync);
    sync();
    return () => {
      state.valueChangedEvent.removeListener(valHandle);
      state.propertiesChangedEvent.removeListener(propHandle);
    };
  }, []);

  return {
    value,
    setValue: (v: boolean) => stateRef.current.setValue(v),
  };
}
