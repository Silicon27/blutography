document.addEventListener('DOMContentLoaded', async () => {
    console.log('Slideshow initialization started');
    
    const slidesContainer = document.getElementById('slides-container');
    const dotsContainer = document.getElementById('dots-container');

    if (!slidesContainer || !dotsContainer) {
        console.error('Required containers not found');
        return;
    }

    let currentSlide = 0;
    let slideInterval;
    let imageNames = [];

    async function init() {
        try {
            console.log('Fetching manifest...');
            const response = await fetch('/assets/manifest.json');
            if (!response.ok) throw new Error('Failed to fetch manifest');
            
            imageNames = await response.json();
            console.log('Manifest loaded:', imageNames);

            if (imageNames.length === 0) {
                throw new Error('Manifest is empty');
            }

            // Initialize placeholders and images
            imageNames.forEach((name, index) => {
                const img = document.createElement('img');
                img.alt = `Slide ${index + 1}`;
                if (index === 0) {
                    img.classList.add('active');
                    // Load first image immediately
                    img.src = `/assets/${name}`;
                }
                slidesContainer.appendChild(img);

                const dot = document.createElement('div');
                dot.classList.add('dot');
                if (index === 0) dot.classList.add('active');
                dot.addEventListener('click', () => {
                    goToSlide(index, true);
                });
                dotsContainer.appendChild(dot);
            });

            // Start loading other images after a short delay to prioritize the first one
            setTimeout(() => {
                const imgElements = slidesContainer.querySelectorAll('img');
                for (let i = 1; i < imageNames.length; i++) {
                    imgElements[i].src = `/assets/${imageNames[i]}`;
                }
            }, 100);
            
            // Start cycle
            startInterval();
        } catch (error) {
            slidesContainer.innerHTML = `<div style="color: white; padding: 20px;">Failed to initialize slideshow. <br><small>Error: ${error.message}</small></div>`;
            console.error('Init failed:', error);
        }
    }

    function goToSlide(index, instant = false) {
        if (index === currentSlide) return;
        
        const imgs = slidesContainer.querySelectorAll('img');
        const dots = dotsContainer.querySelectorAll('.dot');

        if (instant) {
            slidesContainer.classList.add('instant');
        }

        if (imgs[currentSlide]) imgs[currentSlide].classList.remove('active');
        if (dots[currentSlide]) dots[currentSlide].classList.remove('active');

        currentSlide = index;

        if (imgs[currentSlide]) {
            // Ensure src is set if it wasn't already (though it should be)
            if (!imgs[currentSlide].getAttribute('src')) {
                 imgs[currentSlide].src = `/assets/${imageNames[currentSlide]}`;
            }
            imgs[currentSlide].classList.add('active');
        }
        if (dots[currentSlide]) dots[currentSlide].classList.add('active');

        if (instant) {
            void slidesContainer.offsetWidth;
            slidesContainer.classList.remove('instant');
        }

        resetInterval();
    }

    function nextSlide() {
        if (imageNames.length === 0) return;
        const next = (currentSlide + 1) % imageNames.length;
        goToSlide(next);
    }

    function startInterval() {
        if (slideInterval) return;
        slideInterval = setInterval(nextSlide, 5000);
    }

    function resetInterval() {
        clearInterval(slideInterval);
        slideInterval = null;
        startInterval();
    }

    init();
});