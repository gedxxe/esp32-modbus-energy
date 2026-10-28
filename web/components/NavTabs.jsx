import { useLayoutEffect, useRef } from "react";
import { useReducedMotion } from "motion/react";
import StarBorder from "../vendor/react-bits/StarBorder.jsx";
import Icon from "./Icon.jsx";
import { useText } from "../ui.js";

export default function NavTabs({ view, onNavigate }) {
  const { t,lang } = useText();
  const root=useRef(null), selection=useRef(null);
  const reduced=useReducedMotion();
  useLayoutEffect(()=>{
    const nav=root.current,pill=selection.current;
    const place=()=>{
      const button=nav.querySelector('[aria-current="page"]');
      if(!button)return;
      pill.style.width=`${button.offsetWidth}px`;pill.style.height=`${button.offsetHeight}px`;
      pill.style.transform=`translate3d(${button.offsetLeft}px,${button.offsetTop}px,0)`;
    };
    place();const ready=requestAnimationFrame(()=>nav.dataset.ready="true");
    const observer=new ResizeObserver(place);observer.observe(nav);
    return()=>{cancelAnimationFrame(ready);observer.disconnect();};
  },[view,lang,reduced]);
  return <nav ref={root} className="main-nav" aria-label={lang==="id"?"Halaman":"Pages"}>
    <span ref={selection} className="nav-selection" aria-hidden="true"/>
    {["overview","setup","device"].map(page=>{
      const active=view===page;
      const Button=reduced?"button":StarBorder;
      const decoration=reduced?{}:{theme:"light",color:active?"#d5edff":"#8db8ff",backgroundColor:"transparent",textColor:active?"#fff":"#526585",borderColor:"transparent",duration:5,thickness:1.2,radius:14,trailLength:0.18,stars:1,glow:0.18,sparkle:false,hover:active?"brighten":"reveal"};
      return <Button key={page} {...decoration} type="button" className="nav-tab" id={`nav-${page}`} aria-current={active?"page":undefined} onClick={()=>onNavigate(page)}><Icon name={page}/><span>{t(page)}</span></Button>;
    })}
  </nav>;
}
