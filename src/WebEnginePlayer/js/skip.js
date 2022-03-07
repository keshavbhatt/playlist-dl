if(typeof skip_add == "undefined")
{
    const skip_add = (clazz) =>
    {
        const buttons = document.getElementsByClassName(clazz);
        for (const button of buttons)
        {
         button.click();
        }
    }

    setInterval(() =>
    {
        skip_add("ytp-ad-skip-button-text");
        skip_add("ytp-ad-overlay-close-button");
        skip_add("ytp-ad-skip-button");
    }, 300);
}
